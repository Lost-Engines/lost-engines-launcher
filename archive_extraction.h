#pragma once

#include <chrono>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>
#include "platform_defines.h"

struct archive_metadata {
  std::filesystem::path path;
  std::chrono::sys_time<std::chrono::nanoseconds> mtime;
  std::optional<std::filesystem::perms> permissions;
  bool directory{false};
};

auto resolve_archive_path(std::filesystem::path const &root, std::string const &name)->std::filesystem::path;
void restore_archive_metadata(std::filesystem::path const &root, std::vector<archive_metadata> const &entries);

#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
void extract_tar(std::istream &input, std::filesystem::path const &root);
#endif
