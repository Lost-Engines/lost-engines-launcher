#include "get_game_download_url.h"
#include "platform_defines.h"

boost::urls::url get_game_download_url(nlohmann::json const &game_info) {
  /// Helper to construct a platform-specific game download URL for a github release based on our json format
  // example resource URL: https://github.com/Lost-Engines/apache-cpp/releases/latest/download/apache-linux.tar.gz
  return boost::urls::url{
    game_info["repo_url"].get<std::string>() +
    "/releases/latest/download/" +
    game_info["packages"]
    #if defined(PLATFORM_WINDOWS)
      ["windows"]
    #elif defined(PLATFORM_LINUX)
      ["linux"]
    #elif defined(PLATFORM_MACOS)
      #if defined(__aarch64__)
        ["macos_arm"]
      #elif defined(__x86_64__)
        ["macos_intel"]
      #else
        #error "Unknown macos architecture"
      #endif
    #else
      #error "Platform must be defined for download specification"
    #endif
    .get<std::string>()
  };
}
