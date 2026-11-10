#include "platform_paths.h"
#include "platform_defines.h"

namespace platform_paths {

std::filesystem::path get_user_config_dir() {
  /// Return the user preferences directory for this platform
  #if defined(PLATFORM_LINUX)
    return std::filesystem::path{getenv("HOME")} / ".config";
  #elif defined(PLATFORM_WINDOWS)
    return std::filesystem::path{getenv("APPDATA")} ;
  #elif defined(PLATFORM_MACOS)
    return std::filesystem::path{getenv("HOME")} / "Library" / "Application Support";
  #else
    return {};
  #endif
}

std::filesystem::path get_user_data_dir() {
  /// Return the user data directory for this platform
  #if defined(PLATFORM_LINUX)
    return std::filesystem::path{getenv("HOME")} / ".local" / "share";
  #elif defined(PLATFORM_WINDOWS)
    return std::filesystem::path{getenv("LOCALAPPDATA")} ;
  #elif defined(PLATFORM_MACOS)
    return std::filesystem::path{getenv("HOME")} / "Library" / "Application Support";
  #else
    return {};
  #endif
}

std::filesystem::path get_user_cache_dir() {
  #if defined(PLATFORM_LINUX)
    return std::filesystem::path{getenv("HOME")} / ".cache";
  #elif defined(PLATFORM_WINDOWS)
    return std::filesystem::path{getenv("LOCALAPPDATA")} / "Cache";
  #elif defined(PLATFORM_MACOS)
    return std::filesystem::path{getenv("HOME")} / "Library" / "Caches";
  #else
    return {};
  #endif
}

} // namespace platform_paths
