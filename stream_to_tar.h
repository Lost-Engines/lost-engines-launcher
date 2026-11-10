#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <istream>
#include <limits>
#include <map>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <type_traits>

enum class stream_to_tar_entry_type {
  file,
  directory,
  symbolic_link,
  hard_link,
};

struct stream_to_tar_entry {
  /// Properties of a file read from the TAR stream
  std::string filename;                                                       /// name of the file, including its directory
  uint64_t size{0u};                                                          /// size of the file's contents in bytes
  uint32_t mode{0u};                                                          /// file permission bits from the TAR header
  stream_to_tar_entry_type type{stream_to_tar_entry_type::file};               /// kind of archive entry
  std::string linkname;                                                       /// target of a symbolic or hard link
  int64_t uid{0};                                                             /// numeric owner ID, exposed without applying it
  int64_t gid{0};                                                             /// numeric group ID, exposed without applying it
  std::string uname;                                                          /// owner name, when present
  std::string gname;                                                          /// group name, when present
  std::chrono::sys_time<std::chrono::nanoseconds> mtime;                        /// modification time since the Unix epoch
  std::map<std::string, std::string> extended_headers;                          /// effective extended attributes, including unrecognised keys
};

namespace stream_to_tar_detail {

template<typename T>
auto parse_number(std::string_view field, int const base = 8)->T {
  /// Read an ASCII number or a GNU base-256 number, checking the destination's range
  if(!field.empty() && base == 8 && (static_cast<unsigned char>(field.front()) & 0x80u) != 0u) {
    bool const negative{(static_cast<unsigned char>(field.front()) & 0x40u) != 0u};
    if(negative && !std::is_signed_v<T>) throw std::runtime_error{"Negative value in unsigned TAR field"};
    uint64_t constexpr limit{static_cast<uint64_t>(std::numeric_limits<T>::max())};
    uint64_t value{0u};
    for(size_t i{0}; i != field.size(); ++i) {
      unsigned int byte{static_cast<unsigned char>(field[i])};
      if(negative) byte ^= 0xFFu;                                             // accumulate the complement, then negate below
      if(i == 0) byte &= 0x7Fu;                                               // discard the base-256 marker
      if(value > (limit - byte) / 256u) throw std::runtime_error{"TAR numeric field is out of range"};
      value = value * 256u + byte;
    }
    if constexpr (std::is_signed_v<T>) {
      if(negative) return -static_cast<T>(value) - 1;
    }
    return static_cast<T>(value);
  }

  while(!field.empty() && (field.front() == ' ' || field.front() == '\0')) field.remove_prefix(1);
  while(!field.empty() && (field.back() == ' ' || field.back() == '\0')) field.remove_suffix(1);
  if(field.empty()) return 0;

  T value{0};
  auto const [end, error]{std::from_chars(field.data(), field.data() + field.size(), value, base)};
  if(error != std::errc{} || end != field.data() + field.size()) throw std::runtime_error{"Invalid numeric value in TAR header"};
  return value;
}

inline auto make_timestamp(uint64_t const seconds, uint64_t const fraction, bool const negative)->std::chrono::sys_time<std::chrono::nanoseconds> {
  /// Convert an absolute seconds/fraction pair without overflowing a nanosecond timestamp
  uint64_t const limit{static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + (negative ? 1u : 0u)};
  if(seconds > limit / 1'000'000'000u || fraction > limit - seconds * 1'000'000'000u) {
    throw std::runtime_error{"TAR modification time is out of range"};
  }
  auto const magnitude{seconds * 1'000'000'000u + fraction};
  int64_t const count{negative && magnitude != 0 ? -static_cast<int64_t>(magnitude - 1u) - 1 : static_cast<int64_t>(magnitude)};
  return std::chrono::sys_time<std::chrono::nanoseconds>{std::chrono::nanoseconds{count}};
}

inline auto parse_timestamp(std::string_view value)->std::chrono::sys_time<std::chrono::nanoseconds> {
  /// PAX timestamps are decimal seconds, optionally signed and fractional
  bool const negative{!value.empty() && value.front() == '-'};
  if(negative) value.remove_prefix(1);
  auto const point{value.find('.')};
  auto const whole{value.substr(0, point)};
  if(whole.empty()) throw std::runtime_error{"Invalid PAX modification time"};
  auto const seconds{parse_number<uint64_t>(whole, 10)};
  uint64_t fraction{0u};
  if(point != std::string_view::npos) {
    auto const digits{value.substr(point + 1)};
    if(digits.empty()) throw std::runtime_error{"Invalid PAX modification time"};
    bool remainder{false};
    for(size_t i{0}; i != digits.size(); ++i) {
      if(digits[i] < '0' || digits[i] > '9') throw std::runtime_error{"Invalid PAX modification time"};
      if(i < 9) fraction = fraction * 10u + static_cast<unsigned int>(digits[i] - '0');
      else if(digits[i] != '0') remainder = true;
    }
    for(size_t i{digits.size()}; i < 9; ++i) fraction *= 10u;
    if(negative && remainder) ++fraction;                                     // round discarded precision towards the earlier instant
  }
  return make_timestamp(seconds, fraction, negative);
}

inline void parse_pax(std::string_view payload, std::map<std::string, std::string> &attributes, bool const global) {
  /// Lengths include the complete record; values may themselves contain newlines
  while(!payload.empty()) {
    auto const space{payload.find(' ')};
    if(space == std::string_view::npos || space == 0) throw std::runtime_error{"Invalid PAX record length"};
    auto const length{parse_number<uint64_t>(payload.substr(0, space), 10)};
    if(length > payload.size() || length <= space + 3u) throw std::runtime_error{"Invalid PAX record length"};
    auto const record{payload.substr(space + 1, static_cast<size_t>(length) - space - 1)};
    auto const equal{record.find('=')};
    if(record.back() != '\n' || equal == std::string_view::npos || equal == 0) throw std::runtime_error{"Invalid PAX record"};
    std::string const key{record.substr(0, equal)};
    std::string const value{record.substr(equal + 1, record.size() - equal - 2)};
    if(global && value.empty()) attributes.erase(key);
    else attributes[key] = value;                                            // an empty per-file value suppresses the global value for that file
    payload.remove_prefix(static_cast<size_t>(length));
  }
}

} // namespace stream_to_tar_detail

template<typename T>
void stream_to_tar(std::istream &stream,                                      /// stream containing uncompressed TAR data
                   T &&on_entry) {                                          /// callback receiving entry properties and a bounded content stream
  /// Read a TAR archive from a stream, passing each file to the callback without buffering the whole file
  class entry_buffer : public std::streambuf {
    std::istream &source;
    uint64_t remaining;
    std::array<char, 16384> buffer;

  public:
    entry_buffer(std::istream &input, uint64_t const size)
      : source(input),
        remaining(size) {}

  private:
    auto underflow()->int_type override {
      if(gptr() != egptr()) return traits_type::to_int_type(*gptr());
      if(remaining == 0) return traits_type::eof();

      auto const count{static_cast<std::streamsize>(std::min<uint64_t>(remaining, buffer.size()))};
      source.read(buffer.data(), count);
      if(source.gcount() != count) throw std::runtime_error{"Truncated TAR file contents"};
      remaining -= static_cast<uint64_t>(count);
      setg(buffer.data(), buffer.data(), buffer.data() + count);
      return traits_type::to_int_type(*gptr());
    }
  };

  auto field_text{[](std::string_view const field) {
    return std::string{field.substr(0, field.find('\0'))};
  }};
  auto skip_padding{[&stream](uint64_t const size) {
    auto const padding{static_cast<std::streamsize>((512u - size % 512u) % 512u)};
    stream.ignore(padding);
    if(stream.gcount() != padding) throw std::runtime_error{"Truncated TAR file padding"};
  }};
  std::map<std::string, std::string> global_headers;
  std::map<std::string, std::string> local_headers;
  bool pending_header{false};

  for(;;) {
    struct {                                                                 // offset
      char name[100];                                                        //   0    filename
      char mode[8];                                                          // 100    file mode
      char uid[8];                                                           // 108    user ID
      char gid[8];                                                           // 116    group ID
      char size[12];                                                         // 124    file size, in octal
      char mtime[12];                                                        // 136    modification time
      char chksum[8];                                                        // 148    header checksum
      char typeflag;                                                         // 156    file type
      char linkname[100];                                                    // 157    link target
      char magic[6];                                                         // 257    TAR format identifier
      char version[2];                                                       // 263    format version
      char uname[32];                                                        // 265    user name
      char gname[32];                                                        // 297    group name
      char devmajor[8];                                                      // 329    device major number
      char devminor[8];                                                      // 337    device minor number
      char prefix[155];                                                      // 345    USTAR filename prefix
      char padding[12];                                                      // 500    padding to reach 512 bytes
    } header;                                                                // 512
    static_assert(sizeof(header) == 512);

    auto constexpr header_size{static_cast<std::streamsize>(sizeof(header))};
    stream.read(reinterpret_cast<char*>(&header), header_size);
    if(stream.gcount() == 0 && stream.eof() && !stream.bad()) {                 // also accept archives without the optional zero tail
      if(pending_header) throw std::runtime_error{"TAR metadata has no following entry"};
      return;
    }
    if(stream.gcount() != header_size) throw std::runtime_error{"Truncated TAR header"};

    auto const bytes{reinterpret_cast<unsigned char const*>(&header)};
    if(std::all_of(bytes, bytes + sizeof(header), [](unsigned char const byte){return byte == 0;})) {
      if(pending_header) throw std::runtime_error{"TAR metadata has no following entry"};
      stream.ignore(std::numeric_limits<std::streamsize>::max());              // consume the tail, allowing a gzip stream to finish and check its trailer
      if(stream.bad()) throw std::runtime_error{"Failed to finish reading TAR stream"};
      return;
    }

    unsigned int checksum{0u};
    int signed_checksum{0};
    for(size_t i{0}; i != sizeof(header); ++i) {
      unsigned int const byte{(i >= 148 && i < 156) ? static_cast<unsigned char>(' ') : bytes[i]};
      checksum += byte;
      signed_checksum += byte < 128u ? static_cast<int>(byte) : static_cast<int>(byte) - 256;
    }
    auto const expected{stream_to_tar_detail::parse_number<unsigned int>({header.chksum, sizeof(header.chksum)})};
    if(checksum != expected && (signed_checksum < 0 || static_cast<unsigned int>(signed_checksum) != expected)) {
      throw std::runtime_error{"Invalid TAR header checksum"};
    }

    auto const size{stream_to_tar_detail::parse_number<uint64_t>({header.size, sizeof(header.size)})};
    if(header.typeflag == 'x' || header.typeflag == 'g' || header.typeflag == 'L' || header.typeflag == 'K') {
      if(size > std::numeric_limits<size_t>::max() || size > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error{"TAR metadata is too large"};
      }
      std::string payload(static_cast<size_t>(size), '\0');
      if(size != 0) {
        stream.read(payload.data(), static_cast<std::streamsize>(size));
        if(stream.gcount() != static_cast<std::streamsize>(size)) throw std::runtime_error{"Truncated TAR metadata"};
      }
      skip_padding(size);
      if(header.typeflag == 'g') stream_to_tar_detail::parse_pax(payload, global_headers, true);
      else {
        pending_header = true;
        if(header.typeflag == 'x') stream_to_tar_detail::parse_pax(payload, local_headers, false);
        else if(header.typeflag == 'L') local_headers["path"] = field_text(payload);
        else local_headers["linkpath"] = field_text(payload);
      }
      continue;
    }

    auto const seconds{stream_to_tar_detail::parse_number<int64_t>({header.mtime, sizeof(header.mtime)})};
    uint64_t const magnitude{seconds < 0 ? static_cast<uint64_t>(-(seconds + 1)) + 1u : static_cast<uint64_t>(seconds)};
    stream_to_tar_entry entry{
      .filename{field_text({header.name, sizeof(header.name)})},
      .size{size},
      .mode{stream_to_tar_detail::parse_number<uint32_t>({header.mode, sizeof(header.mode)})},
      .linkname{field_text({header.linkname, sizeof(header.linkname)})},
      .uid{stream_to_tar_detail::parse_number<int64_t>({header.uid, sizeof(header.uid)})},
      .gid{stream_to_tar_detail::parse_number<int64_t>({header.gid, sizeof(header.gid)})},
      .uname{field_text({header.uname, sizeof(header.uname)})},
      .gname{field_text({header.gname, sizeof(header.gname)})},
      .mtime{stream_to_tar_detail::make_timestamp(magnitude, 0u, seconds < 0)},
      .extended_headers{global_headers},
    };
    if(std::string_view{header.magic, sizeof(header.magic)} == std::string_view{"ustar", 6} && header.prefix[0] != '\0') {
      entry.filename = field_text({header.prefix, sizeof(header.prefix)}) + "/" + entry.filename;
    }
    for(auto const &[key, value] : local_headers) {
      if(value.empty()) entry.extended_headers.erase(key);
      else entry.extended_headers[key] = value;
    }
    for(auto const &[key, value] : entry.extended_headers) {
      if(key == "path") entry.filename = value;
      else if(key == "linkpath") entry.linkname = value;
      else if(key == "size") entry.size = stream_to_tar_detail::parse_number<uint64_t>(value, 10);
      else if(key == "uid") entry.uid = stream_to_tar_detail::parse_number<int64_t>(value, 10);
      else if(key == "gid") entry.gid = stream_to_tar_detail::parse_number<int64_t>(value, 10);
      else if(key == "uname") entry.uname = value;
      else if(key == "gname") entry.gname = value;
      else if(key == "mtime") entry.mtime = stream_to_tar_detail::parse_timestamp(value);
      else if(key.starts_with("GNU.sparse.") || key == "SCHILY.realsize" || (key == "SCHILY.filetype" && value == "sparse")) {
        throw std::runtime_error{"Sparse TAR entries are not supported"};
      }
    }
    local_headers.clear();
    pending_header = false;

    switch(header.typeflag) {
    case '\0':
      entry.type = !entry.filename.empty() && entry.filename.back() == '/' ? stream_to_tar_entry_type::directory : stream_to_tar_entry_type::file;
      break;
    case '0':
    case '7':
      break;
    case '1':
      entry.type = stream_to_tar_entry_type::hard_link;
      break;
    case '2':
      entry.type = stream_to_tar_entry_type::symbolic_link;
      break;
    case '5':
      entry.type = stream_to_tar_entry_type::directory;
      break;
    default:
      throw std::runtime_error{"Unsupported TAR entry type: " + std::to_string(static_cast<unsigned char>(header.typeflag))};
    }
    if(entry.type == stream_to_tar_entry_type::directory || entry.type == stream_to_tar_entry_type::symbolic_link) {
      entry.size = 0;                                                        // these entries have no data blocks, regardless of the size field
    }

    entry_buffer buffer{stream, entry.size};
    std::istream contents{&buffer};
    contents.exceptions(std::ios::badbit);
    on_entry(entry, contents);                                               // contents is valid only for the duration of this callback
    if(contents.bad()) throw std::runtime_error{"Failed to read TAR file contents"};
    contents.clear();
    contents.ignore(std::numeric_limits<std::streamsize>::max());              // discard anything the callback did not consume

    skip_padding(entry.size);
  }
}
