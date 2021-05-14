#pragma once
#include <sstream>
#include <string>

namespace jas {
using char_type = char;
using string_type = std::basic_string<char_type>;
using ostream_type = std::basic_ostream<char_type>;
using istream_type = std::basic_istream<char_type>;
using ostringstream_type = std::basic_ostringstream<char_type>;
using istringstream_type = std::basic_istringstream<char_type>;

template <class... _strs>
string_type str_join(_strs&&... ss) {
  ostringstream_type oss;
  (oss << ... << ss);
  return oss.str();
}

}  // namespace jas
