#include "archive_extraction.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <system_error>
#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
  #include <fcntl.h>
  #include <sys/stat.h>
  #include "stream_to_tar.h"
#endif

namespace {

void require_archive_path(std::filesystem::path const &root, std::filesystem::path const &path) {
  /// Include existing links when checking where an archive path leads
  auto const relative{std::filesystem::weakly_canonical(path).lexically_relative(root)};
  if(relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
    throw std::runtime_error{"Archive link must stay within the installation directory"};
  }
}

} // anonymous namespace

auto resolve_archive_path(std::filesystem::path const &root, std::string const &name)->std::filesystem::path {
  std::filesystem::path const path{name};
  if(path.empty() || path.has_root_path() || name.find('\0') != std::string::npos) throw std::runtime_error{"Archive entry must have a relative path"};
  for(auto const &component : path) {
    if(component == "..") throw std::runtime_error{"Archive entry must stay within the installation directory"};
  }
  auto result{(root / path).lexically_normal()};
  if(result != root && result.filename().empty()) result = result.parent_path();
  return result;
}

void restore_archive_metadata(std::filesystem::path const &root, std::vector<archive_metadata> const &entries) {
  /// The final occurrence of a path supplies its metadata; restore directories after their descendants
  std::map<std::filesystem::path, size_t> last_entry;
  for(size_t i{0}; i != entries.size(); ++i) last_entry[entries[i].path] = i;
  std::vector<archive_metadata> metadata;
  for(size_t i{0}; i != entries.size(); ++i) {
    if(last_entry.at(entries[i].path) == i) metadata.push_back(entries[i]);
  }
  std::stable_sort(metadata.begin(), metadata.end(), [](auto const &left, auto const &right) {
    if(left.directory != right.directory) return !left.directory;
    if(!left.directory) return false;
    return std::distance(left.path.begin(), left.path.end()) > std::distance(right.path.begin(), right.path.end());
  });
  for(auto const &entry : metadata) {
    if(entry.path != root) require_archive_path(root, entry.path.parent_path());
    #if defined(PLATFORM_WINDOWS)
      #if __cpp_lib_chrono >= 201907L
        std::filesystem::last_write_time(entry.path, std::chrono::clock_cast<std::chrono::file_clock>(entry.mtime));
      #else
        std::filesystem::last_write_time(entry.path, std::chrono::file_clock::from_sys(entry.mtime));
      #endif
    #elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
      auto const seconds{std::chrono::floor<std::chrono::seconds>(entry.mtime.time_since_epoch())};
      auto const fraction{entry.mtime.time_since_epoch() - seconds};
      timespec const times[2]{
        {.tv_sec{0}, .tv_nsec{UTIME_OMIT}},                                     // preserve access time
        {.tv_sec{static_cast<time_t>(seconds.count())}, .tv_nsec{static_cast<long>(fraction.count())}},
      };
      if(::utimensat(AT_FDCWD, entry.path.c_str(), times, AT_SYMLINK_NOFOLLOW) != 0) {
        throw std::system_error{errno, std::generic_category(), "Failed to restore archive modification time for " + entry.path.string()};
      }
      if(entry.permissions && !std::filesystem::is_symlink(std::filesystem::symlink_status(entry.path))) {
        std::filesystem::permissions(entry.path, *entry.permissions);
      }
    #endif
  }
}

#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
void extract_tar(std::istream &input, std::filesystem::path const &root) {
  /// Restore streamed entries, deferring forward hard links and directory metadata
  struct hard_link {
    std::filesystem::path target;
    bool has_contents{false};
  };
  std::map<std::filesystem::path, hard_link> hard_links;
  std::vector<archive_metadata> metadata;
  auto prepare_file{[&root](std::filesystem::path const &path) {
    if(path == root) throw std::runtime_error{"Archive file cannot replace the installation directory"};
    require_archive_path(root, path.parent_path());
    std::filesystem::create_directories(path.parent_path());
    if(std::filesystem::is_directory(std::filesystem::symlink_status(path))) {
      throw std::runtime_error{"Archive file cannot replace a directory"};
    }
    std::filesystem::remove(path);                                              // replace an existing link rather than writing through it
  }};
  auto resolve_hard_links{[&](bool const final) {
    while(!hard_links.empty()) {
      bool progress{false};
      for(auto link{hard_links.begin()}; link != hard_links.end();) {
        auto const &[path, properties]{*link};
        if(hard_links.contains(properties.target)) {
          ++link;
          continue;
        }
        require_archive_path(root, properties.target);
        auto const status{std::filesystem::symlink_status(properties.target)};
        if(std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status)) {
          prepare_file(path);
          std::filesystem::create_hard_link(properties.target, path);
        } else if(std::filesystem::exists(status)) {
          throw std::runtime_error{"Archive hard link target is not a file: " + properties.target.string()};
        } else if(!final || !properties.has_contents) {
          ++link;
          continue;
        }
        link = hard_links.erase(link);
        progress = true;
      }
      if(!progress) {
        if(!final) return;
        auto const anchor{std::find_if(hard_links.begin(), hard_links.end(), [](auto const &link){return link.second.has_contents;})};
        if(anchor == hard_links.end()) throw std::runtime_error{"Archive has unresolved or cyclic hard links"};
        hard_links.erase(anchor);                                               // a link carrying file contents can supply the inode for a cycle
      }
    }
  }};
  stream_to_tar(input, [&](stream_to_tar_entry const &entry, std::istream &contents) {
    auto const path{resolve_archive_path(root, entry.filename)};
    hard_links.erase(path);                                                     // a later entry replaces any unresolved link at this path
    if(entry.type == stream_to_tar_entry_type::directory) {
      if(path != root) require_archive_path(root, path.parent_path());
      if(std::filesystem::is_symlink(std::filesystem::symlink_status(path))) std::filesystem::remove(path);
      std::filesystem::create_directories(path);
    } else if(entry.type == stream_to_tar_entry_type::symbolic_link) {
      std::filesystem::path const target{entry.linkname};
      if(target.empty() || target.has_root_path() || entry.linkname.find('\0') != std::string::npos) {
        throw std::runtime_error{"Archive symbolic link must have a relative target"};
      }
      require_archive_path(root, path.parent_path() / target);
      prepare_file(path);
      std::filesystem::create_symlink(target, path);
    } else {
      std::filesystem::path target;
      if(entry.type == stream_to_tar_entry_type::hard_link) target = resolve_archive_path(root, entry.linkname);
      prepare_file(path);
      if(entry.type == stream_to_tar_entry_type::file || entry.size != 0) {
        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        output.exceptions(std::ios::badbit | std::ios::failbit);
        if(entry.size != 0) output << contents.rdbuf();
        output.close();
      }
      if(entry.type == stream_to_tar_entry_type::hard_link) {
        if(path == target && entry.size == 0) throw std::runtime_error{"Archive hard link cannot refer to itself without contents"};
        if(path != target) hard_links[path] = {.target{target}, .has_contents{entry.size != 0}};
      }
    }
    metadata.push_back({
      .path{path},
      .mtime{entry.mtime},
      .permissions{static_cast<std::filesystem::perms>(entry.mode & 0777u)},
      .directory{entry.type == stream_to_tar_entry_type::directory},
    });
    resolve_hard_links(false);                                                  // bind links to the target inode as soon as it is available
  });
  resolve_hard_links(true);
  restore_archive_metadata(root, metadata);
}
#endif
