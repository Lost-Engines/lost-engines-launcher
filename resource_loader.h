#pragma once
#include <thread>
#include <boost/asio.hpp>
#include "launcher_state.h"

class resource_loader {
  launcher_state &state;

  std::jthread thread{[&]{
    run();
  }};
  std::vector<std::jthread> network_threads;
  boost::asio::io_context io_context;

public:
  resource_loader(launcher_state &this_state);

  void fetch_game_list();

private:
  void run();
};
