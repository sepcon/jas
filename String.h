#pragma once
#include <sstream>
#include <string>
#include <string_view>

namespace jas {

#ifdef JAS_CHAR_TYPE
using CharType = JAS_CHAR_TYPE;
#else
using CharType = char;
#endif

using String = std::basic_string<CharType>;
using StringView = std::basic_string_view<CharType>;
using OStream = std::basic_ostream<CharType>;
using IStream = std::basic_istream<CharType>;
using OStringStream = std::basic_ostringstream<CharType>;
using IStringStream = std::basic_istringstream<CharType>;

template <class... _Msgs>
String strJoin(_Msgs&&... ss) {
  if constexpr (sizeof...(ss) == 0) {
    return {};
  } else {
    OStringStream oss;
    (oss << ... << ss);
    return oss.str();
  }
}

}  // namespace jas
