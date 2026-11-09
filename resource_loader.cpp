#include "resource_loader.h"
#include <iostream>
#include <boost/beast.hpp>

resource_loader::resource_loader(launcher_state &this_state)
  : state(this_state) {
  /// Default c'tor
  auto const network_thread_count{state.config.get<unsigned int>("network_threads")};
  std::cout << "Starting " << network_thread_count << " network threads" << std::endl;
  for(unsigned int i{0}; i != network_thread_count; ++i) {
    network_threads.emplace_back([&]{
      io_context.run();
    });
  }
}

void resource_loader::fetch_game_list() {
  /// Fetch the latest game list from the repo
  state.game_list_update_state.store(launcher_state::update_state::downloading, std::memory_order_relaxed);
  // TODO: start fetch of game list as per the config, if failed set the game_list_update_state to failed, otherwise complete and set to pending_processing

}

void resource_loader::run() {
  /// Run the resource loader thread
  std::cout << "Starting loader thread" << std::endl;
  // TODO: process queue


  std::cout << "Loader thread finished" << std::endl;
}
