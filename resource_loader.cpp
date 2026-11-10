#include "resource_loader.h"
#include "platform_defines.h"
#include <chrono>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/url/parse.hpp>
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz-cpp/zip_file.hpp>
#undef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "memorystorm/memorystorm.h"
#include "archive_extraction.h"

namespace {

auto download(boost::urls::url url, std::string const accept, std::function<void(float)> const on_progress = {})->boost::asio::awaitable<std::vector<unsigned char>> {
  auto const executor{co_await boost::asio::this_coro::executor};
  auto constexpr timeout{std::chrono::seconds{30}};
  unsigned int constexpr max_redirects{5};
  for(unsigned int redirects{0};; ++redirects) {
    bool const use_tls{url.scheme_id() == boost::urls::scheme::https};
    if((!use_tls && url.scheme_id() != boost::urls::scheme::http) || url.host_address().empty()) {
      throw std::invalid_argument{"Download URL must be an absolute HTTP or HTTPS URL"};
    }

    std::string const host{url.host_address()};
    std::string const port{url.has_port() ? std::string{url.port()} : (use_tls ? "443" : "80")};
    std::string target{url.encoded_target()};
    if(target.empty() || target.front() != '/') target.insert(0, "/");
    std::string authority{url.encoded_host()};
    if(url.has_port()) authority += ":" + port;

    boost::beast::http::request<boost::beast::http::empty_body> request{boost::beast::http::verb::get, target, 11};
    request.set(boost::beast::http::field::host, authority);
    request.set(boost::beast::http::field::user_agent, "Lost-Engines-Launcher");
    request.set(boost::beast::http::field::accept, accept);
    request.keep_alive(false);

    boost::asio::ip::tcp::resolver resolver{executor};
    auto const endpoints{co_await resolver.async_resolve(host, port, boost::asio::use_awaitable)};
    boost::beast::flat_buffer buffer;
    boost::beast::http::response_parser<boost::beast::http::vector_body<unsigned char>> response;
    response.body_limit(boost::none);                                           // archives can exceed Beast's default 8 MiB limit

    auto receive_response{[&](auto &stream)->boost::asio::awaitable<void> {
      boost::beast::get_lowest_layer(stream).expires_after(timeout);
      co_await boost::beast::http::async_write(stream, request, boost::asio::use_awaitable);
      while(!response.is_done()) {
        boost::beast::get_lowest_layer(stream).expires_after(timeout);
        co_await boost::beast::http::async_read_some(stream, buffer, response, boost::asio::use_awaitable);
        if(on_progress && response.is_header_done() && response.get().result() == boost::beast::http::status::ok) {
          if(auto const length{response.content_length()}; length) {
            float const progress{*length == 0 ? 100.0f : static_cast<float>(response.get().body().size()) / static_cast<float>(*length) * 100.0f};
            on_progress(progress);
          }
        }
      }
    }};

    if(use_tls) {
      boost::asio::ssl::context tls_context{boost::asio::ssl::context::tls_client};
      tls_context.set_default_verify_paths();
      tls_context.set_verify_mode(boost::asio::ssl::verify_peer);
      boost::beast::ssl_stream<boost::beast::tcp_stream> stream{executor, tls_context};
      if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
        throw boost::system::system_error{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
      }
      stream.set_verify_callback(boost::asio::ssl::host_name_verification{host});

      boost::beast::get_lowest_layer(stream).expires_after(timeout);
      co_await boost::beast::get_lowest_layer(stream).async_connect(endpoints, boost::asio::use_awaitable);
      boost::beast::get_lowest_layer(stream).expires_after(timeout);
      co_await stream.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::use_awaitable);
      co_await receive_response(stream);

      boost::beast::get_lowest_layer(stream).expires_after(timeout);
      boost::system::error_code error;
      co_await stream.async_shutdown(boost::asio::redirect_error(boost::asio::use_awaitable, error));
      // A complete HTTP message remains valid when the peer omits TLS close_notify.
      if(error && error != boost::asio::error::eof && error != boost::asio::ssl::error::stream_truncated) throw boost::system::system_error{error};
    } else {
      boost::beast::tcp_stream stream{executor};
      stream.expires_after(timeout);
      co_await stream.async_connect(endpoints, boost::asio::use_awaitable);
      co_await receive_response(stream);
      boost::system::error_code error;
      stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
    }

    auto const status{response.get().result()};
    if(status == boost::beast::http::status::moved_permanently ||
       status == boost::beast::http::status::found ||
       status == boost::beast::http::status::see_other ||
       status == boost::beast::http::status::temporary_redirect ||
       status == boost::beast::http::status::permanent_redirect) {
      if(redirects == max_redirects) throw std::runtime_error{"Too many download redirects"};
      auto const location{response.get()[boost::beast::http::field::location]};
      if(location.empty()) throw std::runtime_error{"Download redirect has no Location header"};
      url.resolve(boost::urls::parse_uri_reference(location).value()).value();
      continue;
    }
    if(status != boost::beast::http::status::ok) {
      throw std::runtime_error{"Download returned HTTP " + std::to_string(response.get().result_int())};
    }

    co_return std::move(response.get().body());
  }
}

} // anonymous namespace

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

  boost::asio::co_spawn(
    boost::asio::make_strand(io_context),
    [this]->boost::asio::awaitable<void> {
      auto const data{co_await download(state.game_list_url, "application/json")};
      state.game_list_update_state.store(launcher_state::update_state::updating, std::memory_order_release);

      auto new_list = nlohmann::ordered_json::parse(data.begin(), data.end());
      if(new_list["games"].empty()) throw std::runtime_error{"Game list file does not contain the expected game list data"};
      *state.game_list = new_list;
      for(auto const &game : new_list["games"]) {
        state.selected_game_versions[game["name"].get<std::string>()] = game["preferred_version"].get<std::string>();
      }

      std::ofstream{state.game_list_file} << new_list.dump(2);
      std::cout << "Game list updated with " << new_list["games"].size() << " games and saved in " << state.game_list_file << std::endl;
      state.game_list_update_state.store(launcher_state::update_state::up_to_date, std::memory_order_release);
    },
    [this](std::exception_ptr const error) {
      if(error) {
        try {
          std::rethrow_exception(error);
        } catch(std::exception const &exception) {
          std::cerr << "Failed to fetch game list from " << state.game_list_url << ": " << exception.what() << std::endl;
        }
        state.game_list_update_state.store(launcher_state::update_state::failed, std::memory_order_release);
        return;
      }
      state.game_list_update_state.store(launcher_state::update_state::up_to_date, std::memory_order_release);
    }
  );
}

void resource_loader::install_game(boost::urls::url const &game_download_url, std::filesystem::path const &destination, launcher_state::game_files_state &files_state) {
  /// Fetch the game package from the given URL and extract it at the destination
  std::cout << "Downloading from " << game_download_url << std::endl;
  files_state.download_progress.store(-1.0f, std::memory_order_relaxed);        // unknown until Content-Length is received
  files_state.game_download_state.store(launcher_state::game_files_state::download_states::in_progress, std::memory_order_release);

  boost::asio::co_spawn(
    boost::asio::make_strand(io_context),
    [game_download_url, destination, &files_state]->boost::asio::awaitable<void> {
      auto const archive{co_await download(game_download_url, "application/octet-stream", [&files_state](float const progress) {
        files_state.download_progress.store(progress, std::memory_order_relaxed);
      })};
      files_state.game_download_state.store(launcher_state::game_files_state::download_states::done, std::memory_order_release);
      std::cout << "Downloaded " << memorystorm::human_readable(archive.size()) << ", installing in " << destination << std::endl;
      std::filesystem::create_directories(destination);
      auto const root{std::filesystem::canonical(destination)};

      #if defined(PLATFORM_WINDOWS)
        miniz_cpp::zip_file package{archive};
        std::vector<archive_metadata> metadata;
        for(auto const &entry : package.infolist()) {
          auto const path{resolve_archive_path(root, entry.filename)};
          std::tm time{};
          time.tm_sec = entry.date_time.seconds;
          time.tm_min = entry.date_time.minutes;
          time.tm_hour = entry.date_time.hours;
          time.tm_mday = entry.date_time.day;
          time.tm_mon = entry.date_time.month - 1;
          time.tm_year = entry.date_time.year - 1900;
          time.tm_isdst = -1;
          auto const modified{std::mktime(&time)};
          if(modified == std::time_t{-1}) throw std::runtime_error{"Invalid ZIP modification time"};
          bool const directory{entry.filename.back() == '/'};
          metadata.push_back({.path{path}, .mtime{std::chrono::system_clock::from_time_t(modified)}, .permissions{std::nullopt}, .directory{directory}});
          if(directory) {
            std::filesystem::create_directories(path);
            continue;
          }
          std::filesystem::create_directories(path.parent_path());
          std::ofstream output{path, std::ios::binary | std::ios::trunc};
          output.exceptions(std::ios::badbit | std::ios::failbit);
          output << package.read(entry);
          output.close();
        }
        restore_archive_metadata(root, metadata);
      #elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
        boost::iostreams::filtering_istream input;
        input.push(boost::iostreams::gzip_decompressor{});
        input.push(boost::iostreams::array_source{reinterpret_cast<char const*>(archive.data()), archive.size()});
        input.exceptions(std::ios::badbit);
        extract_tar(input, root);
      #else
        #error "Platform must be defined for installation"
      #endif

      files_state.data_download_state.store(launcher_state::game_files_state::download_states::in_progress, std::memory_order_release);
      // TODO: get data files or mark not_needed if none
      files_state.data_download_state.store(launcher_state::game_files_state::download_states::done, std::memory_order_release);
    },
    [game_download_url, &files_state](std::exception_ptr const error) {
      if(!error) return;
      files_state.game_download_state.store(launcher_state::game_files_state::download_states::failed, std::memory_order_release);
      try {
        std::rethrow_exception(error);
      } catch(std::exception const &exception) {
        std::cerr << "Failed to download game from " << game_download_url << ": " << exception.what() << std::endl;
      }
    }
  );
}

void resource_loader::run() {
  /// Run the resource loader thread
  std::cout << "Starting loader thread" << std::endl;
  // TODO: process queue


  std::cout << "Loader thread finished" << std::endl;
}
