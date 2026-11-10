#pragma once

#include <filesystem>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/thread/synchronized_value.hpp>
#include <boost/url/urls.hpp>
#include <nlohmann/json.hpp>
#include "platform_paths.h"

struct launcher_state {
  enum class update_state {
    not_tried,
    loading,
    failed_to_connect,
    outdated,
    downloading,
    updating,
    up_to_date,
  };
  std::atomic<update_state> launcher_update_state{update_state::not_tried};
  std::atomic<update_state> game_list_update_state{update_state::not_tried};

  std::filesystem::path config_dir{platform_paths::get_user_config_dir() / "lost-engines"};
  std::filesystem::path game_list_file;
  boost::urls::url game_list_url;

  boost::synchronized_value<nlohmann::ordered_json> game_list;

  std::unordered_map<std::string, std::string> selected_game_versions;

  boost::property_tree::ptree config;
};
