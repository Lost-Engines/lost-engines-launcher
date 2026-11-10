#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>

std::filesystem::path get_game_executable_path(const nlohmann::json &game_info);
