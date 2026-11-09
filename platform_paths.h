#pragma once

#include <filesystem>

namespace platform_paths {

std::filesystem::path get_user_config_dir();
std::filesystem::path get_user_data_dir();
std::filesystem::path get_user_cache_dir();

} // namespace platform_paths
