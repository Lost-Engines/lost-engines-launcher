#include "load_config.h"
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

void load_config(launcher_state &state) {
  std::cout << "Configuration directory: " << state.config_dir << std::endl;

  auto config_path{state.config_dir / "config.ini"};

  if(std::filesystem::exists(config_path)) {
    std::cout << "Loading configuration from " << config_path << std::endl;
    boost::property_tree::ini_parser::read_ini(config_path, state.config);
  } else {
    if(!std::filesystem::exists(state.config_dir)) {
      std::cout << "Creating configuration directory " << state.config_dir << std::endl;
      std::filesystem::create_directories(state.config_dir);
    }
    std::cout << "Creating default configuration in " << config_path << std::endl;
  }

  if(state.config.count("default_data_dir") == 0) {
    std::filesystem::path default_data_dir{platform_paths::get_user_data_dir() / "lost-engines"};
    state.config.put("default_data_dir", default_data_dir.string());
  }
  if(state.config.count("cache_dir") == 0) {
    std::filesystem::path cache_dir{platform_paths::get_user_cache_dir() / "lost-engines"};
    state.config.put("cache_dir", cache_dir.string());
  }
  if(state.config.count("launcher_repo_url") == 0) {
    state.config.put("launcher_repo_url", "https://github.com/Lost-Engines/lost-engines-launcher");
  }
  if(state.config.count("launcher_repo_raw_url") == 0) {
    state.config.put("launcher_repo_raw_url", "https://raw.githubusercontent.com/Lost-Engines/lost-engines-launcher/refs/heads/master");
  }
  if(state.config.count("network_threads") == 0) {
    unsigned int const max_hardware_threads{std::thread::hardware_concurrency()};
    state.config.put("network_threads", std::clamp(max_hardware_threads, 1u, 4u));
  }

  state.game_list_file = state.config.get<std::filesystem::path>("cache_dir") / "game_list.json";
  state.game_list_url = boost::urls::url{state.config.get<std::string>("launcher_repo_raw_url")};
  state.game_list_url.segments().push_back("game_list.json");
  boost::property_tree::ini_parser::write_ini(config_path, state.config);

  std::cout << "Default data dir: " << state.config.get<std::filesystem::path>("default_data_dir") << std::endl;
  std::cout << "Launcher repository: " << state.config.get<std::string>("launcher_repo_url") << std::endl;
  std::cout << "Game list download: " << state.game_list_url << std::endl;
  std::cout << "Game list cache: " << state.game_list_file << std::endl;
}
