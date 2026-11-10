#include "resource_loader.h"
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;

resource_loader::resource_loader(launcher_state &this_state)
  : state(this_state) {
  /// Default c'tor
  auto const network_thread_count{state.config.get<unsigned int>("network_threads")};
  if(network_thread_count == 0) throw std::runtime_error{"Network thread count cannot be 0, adjust your configuration"};
  std::cout << "Starting " << network_thread_count << " network threads" << std::endl;
  for(unsigned int i{0}; i != network_thread_count; ++i) {
    network_threads.emplace_back([this](std::stop_token const stop_token){
      std::stop_callback stop_network{stop_token, [this]{io_context.stop();}};
      io_context.run();
    });
  }
}

void resource_loader::fetch_game_list() {
  /// Fetch the latest game list from the repo
  if(state.game_list_update_state.exchange(launcher_state::update_state::downloading, std::memory_order_relaxed) == launcher_state::update_state::downloading) return;
  std::cout << "Fetching game list" << std::endl;

  net::co_spawn(net::make_strand(io_context), [this]()->net::awaitable<void> {
    auto const url{state.game_list_url};
    bool const use_tls{url.scheme_id() == boost::urls::scheme::https};
    if((!use_tls && url.scheme_id() != boost::urls::scheme::http) || url.host_address().empty()) {
      throw std::invalid_argument{"Game list URL must be an absolute HTTP or HTTPS URL"};
    }

    std::string const host{url.host_address()};
    std::string const port{url.has_port() ? std::string{url.port()} : (use_tls ? "443" : "80")};
    std::string target{url.encoded_target()};
    if(target.empty() || target.front() != '/') target.insert(0, "/");
    std::string authority{url.encoded_host()};
    if(url.has_port()) authority += ":" + port;

    http::request<http::empty_body> request{http::verb::get, target, 11};
    request.set(http::field::host, authority);
    request.set(http::field::user_agent, "Lost-Engines-Launcher");
    request.set(http::field::accept, "application/json");
    request.keep_alive(false);

    auto const executor{co_await net::this_coro::executor};
    net::ip::tcp::resolver resolver{executor};
    auto const endpoints{co_await resolver.async_resolve(host, port, net::use_awaitable)};
    auto constexpr timeout{std::chrono::seconds{30}};
    beast::flat_buffer buffer;
    http::response<http::string_body> response;

    auto receive_response{[&](auto &stream)->net::awaitable<void> {
      beast::get_lowest_layer(stream).expires_after(timeout);
      co_await http::async_write(stream, request, net::use_awaitable);
      beast::get_lowest_layer(stream).expires_after(timeout);
      co_await http::async_read(stream, buffer, response, net::use_awaitable);
    }};

    if(use_tls) {
      ssl::context tls_context{ssl::context::tls_client};
      tls_context.set_default_verify_paths();
      tls_context.set_verify_mode(ssl::verify_peer);
      beast::ssl_stream<beast::tcp_stream> stream{executor, tls_context};
      if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
        throw boost::system::system_error{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
      }
      stream.set_verify_callback(ssl::host_name_verification{host});

      beast::get_lowest_layer(stream).expires_after(timeout);
      co_await beast::get_lowest_layer(stream).async_connect(endpoints, net::use_awaitable);
      beast::get_lowest_layer(stream).expires_after(timeout);
      co_await stream.async_handshake(ssl::stream_base::client, net::use_awaitable);
      co_await receive_response(stream);

      beast::get_lowest_layer(stream).expires_after(timeout);
      boost::system::error_code error;
      co_await stream.async_shutdown(net::redirect_error(net::use_awaitable, error));
      // A complete HTTP message remains valid when the peer omits TLS close_notify.
      if(error && error != net::error::eof && error != ssl::error::stream_truncated) throw boost::system::system_error{error};
    } else {
      beast::tcp_stream stream{executor};
      stream.expires_after(timeout);
      co_await stream.async_connect(endpoints, net::use_awaitable);
      co_await receive_response(stream);
      boost::system::error_code error;
      stream.socket().shutdown(net::ip::tcp::socket::shutdown_both, error);
    }

    if(response.result() != http::status::ok) {
      throw std::runtime_error{"Game list request returned HTTP " + std::to_string(response.result_int())};
    }
    auto new_list = nlohmann::ordered_json::parse(response.body());
    if(new_list["games"].empty()) throw std::runtime_error{"Game list file does not contain the expected game list data"};
    *state.game_list = new_list;
  }, [this](std::exception_ptr const error) {
    if(error) {
      try {
        std::rethrow_exception(error);
      } catch(std::exception const &exception) {
        std::cerr << "Failed to fetch game list from " << state.game_list_url << ": " << exception.what() << std::endl;
      }
      state.game_list_update_state.store(launcher_state::update_state::failed_to_connect, std::memory_order_release);
      return;
    }
    state.game_list_update_state.store(launcher_state::update_state::up_to_date, std::memory_order_release);
  });
}

void resource_loader::run() {
  /// Run the resource loader thread
  std::cout << "Starting loader thread" << std::endl;
  // TODO: process queue


  std::cout << "Loader thread finished" << std::endl;
}
