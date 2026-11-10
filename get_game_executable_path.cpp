#include "get_game_executable_path.h"
#include "platform_defines.h"

std::filesystem::path get_game_executable_path(const nlohmann::json &game_info) {
  return game_info["executables"]
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
  .get<std::string>();
}
