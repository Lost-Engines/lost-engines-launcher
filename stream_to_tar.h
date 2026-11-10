#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <istream>
#include <limits>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>

struct stream_to_tar_entry {
  /// Properties of a file read from the TAR stream
  std::string filename;                                                       /// name of the file, including its directory
  uint64_t size{0u};                                                          /// size of the file's contents in bytes
  bool is_directory{false};                                                   /// whether the entry represents a directory
};

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

  auto parse_octal{[](std::string_view field)->uint64_t {
    while(!field.empty() && (field.front() == ' ' || field.front() == '\0')) field.remove_prefix(1);
    while(!field.empty() && (field.back() == ' ' || field.back() == '\0')) field.remove_suffix(1);
    if(field.empty()) return 0;

    uint64_t value{0u};
    auto const [end, error]{std::from_chars(field.data(), field.data() + field.size(), value, 8)};
    if(error != std::errc{} || end != field.data() + field.size()) throw std::runtime_error{"Invalid octal value in TAR header"};
    return value;
  }};
  auto field_text{[](std::string_view const field) {
    return std::string{field.substr(0, field.find('\0'))};
  }};

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
    if(stream.gcount() == 0 && stream.eof() && !stream.bad()) return;           // also accept archives without the optional zero tail
    if(stream.gcount() != header_size) throw std::runtime_error{"Truncated TAR header"};

    auto const bytes{reinterpret_cast<unsigned char const*>(&header)};
    if(std::all_of(bytes, bytes + sizeof(header), [](unsigned char const byte){return byte == 0;})) {
      stream.ignore(std::numeric_limits<std::streamsize>::max());              // consume the tail, allowing a gzip stream to finish and check its trailer
      if(stream.bad()) throw std::runtime_error{"Failed to finish reading TAR stream"};
      return;
    }

    unsigned int checksum{0u};
    for(size_t i{0}; i != sizeof(header); ++i) checksum += (i >= 148 && i < 156) ? static_cast<unsigned char>(' ') : bytes[i];
    if(checksum != parse_octal({header.chksum, sizeof(header.chksum)})) throw std::runtime_error{"Invalid TAR header checksum"};

    stream_to_tar_entry entry{
      .filename{field_text({header.name, sizeof(header.name)})},
      .size{parse_octal({header.size, sizeof(header.size)})},
      .is_directory{header.typeflag == '5'},
    };
    if(header.magic[5] == '\0' && header.prefix[0] != '\0') {
      entry.filename = field_text({header.prefix, sizeof(header.prefix)}) + "/" + entry.filename;
    }
    bool const metadata{header.typeflag == 'x' || header.typeflag == 'g'};
    if(!metadata && !entry.is_directory && header.typeflag != '0' && header.typeflag != '\0') {
      throw std::runtime_error{"Unsupported TAR entry type"};
    }

    entry_buffer buffer{stream, entry.size};
    std::istream contents{&buffer};
    contents.exceptions(std::ios::badbit);
    if(!metadata) on_entry(entry, contents);                                   // contents is valid only for the duration of this callback
    if(contents.bad()) throw std::runtime_error{"Failed to read TAR file contents"};
    contents.clear();
    contents.ignore(std::numeric_limits<std::streamsize>::max());              // discard anything the callback did not consume

    auto const padding{static_cast<std::streamsize>((512u - entry.size % 512u) % 512u)};
    stream.ignore(padding);
    if(stream.gcount() != padding) throw std::runtime_error{"Truncated TAR file padding"};
  }
}
