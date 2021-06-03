#pragma once

#include <iostream>

#include "Json.h"

namespace jas {

#ifdef JAS_USE_WSTR
#define __clogger std::wcout
#else
#define __clogger std::cout
#endif

struct __Clogger {
  template <typename T>
  const __Clogger& operator<<(const T& v) const {
    __clogger << v;
    return *this;
  }
  ~__Clogger() { __clogger << "\n"; }
};

auto clogger() { return __Clogger{}; }

OStream& operator<<(OStream& os, const JsonAdapter& e) {
  os << JsonTrait::dump(e);
  return os;
}

}  // namespace jas
