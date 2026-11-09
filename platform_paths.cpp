#include "platform_paths.h"

namespace platform_paths {

std::filesystem::path get_user_config_dir() {
  /// Return the user preferences directory for this platform
  #if defined(__linux__) || defined(__unix__)
    return std::filesystem::path{getenv("HOME")} / ".config";
  #elif defined(_WIN32)
    return std::filesystem::path{getenv("APPDATA")} ;
  #elif defined(__APPLE__)
    return std::filesystem::path{getenv("HOME")} / "Library" / "Application Support";
  #else
    return {};
  #endif
}

std::filesystem::path get_user_data_dir() {
  /// Return the user data directory for this platform
  #if defined(__linux__) || defined(__unix__)
    return std::filesystem::path{getenv("HOME")} / ".local" / "share";
  #elif defined(_WIN32)
    return std::filesystem::path{getenv("LOCALAPPDATA")} ;
  #elif defined(__APPLE__)
    return std::filesystem::path{getenv("HOME")} / "Library" / "Application Support";
  #else
    return {};
  #endif
}

std::filesystem::path get_user_cache_dir() {
  #if defined(__linux__) || defined(__unix__)
    return std::filesystem::path{getenv("HOME")} / ".cache";
  #elif defined(_WIN32)
    return std::filesystem::path{getenv("LOCALAPPDATA")} / "Cache";
  #elif defined(__APPLE__)
    return std::filesystem::path{getenv("HOME")} / "Library" / "Caches";
  #else
    return {};
  #endif
}

} // namespace platform_paths
