#pragma once
#include <stop_token>
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include "launcher_state.h"

class resource_loader {
  launcher_state &state;

  boost::asio::io_context io_context;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> network_work{boost::asio::make_work_guard(io_context)};
  std::vector<std::jthread> network_threads;
  std::jthread thread{[this]{
    run();
  }};

public:
  resource_loader(launcher_state &this_state);

  void fetch_game_list();

  void install_game(boost::urls::url const &game_download_url, std::filesystem::path const &destination, launcher_state::game_files_state &files_state);

private:
  void run();
};
