#pragma once
#include <sstream>
#include <string>

namespace jas {

#ifdef JAS_USE_WSTR
using CharType = wchar_t;
#define JASSTR(cstr) L##cstr
#else
using CharType = char;
#define JASSTR(cstr) cstr
#endif

using String = std::basic_string<CharType>;
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
