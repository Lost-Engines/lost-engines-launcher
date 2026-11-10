#pragma once

#include <sstream>
#include <string>

// Example usage:
//   For use where you'd want to build a std::string or c_string in-place using stream operators:
//     throw std::runtime_error(std::string{stream_string{} << "whatever" << some_variable});

class stream_string {
  /// Utility to allow temporary string and C-string construction using a stream
  std::ostringstream oss;
  std::string str;

public:
  template<typename T>
  stream_string &operator<<(T const &in){
    /// Stream operator
    oss << in;
    return *this;
  }

  operator std::string() {
    /// Conversion operator to std::string
    return oss.str();
  }

  char const *c_str() {
    /// Convert to C string
    str = oss.str();
    return str.c_str();
  }

  operator char const*() {
    /// Convert to C string
    return c_str();
  }
};
