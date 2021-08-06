#pragma once

#include <chrono>
#include <iomanip>
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
  ~__Clogger() { __clogger << std::endl; /*"\n";*/ }
};

inline auto clogger() { return __Clogger{}; }

inline OStream& operator<<(OStream& os, const JsonAdapter& e) {
  os << JsonTrait::dump(e);
  return os;
}

struct CloggerSection {
  CloggerSection(const String& sectionName) {
    __clogger << "==> " << sectionName << ":\n"
              << std::setfill(JASSTR('-')) << std::setw(sectionName.size() + 6)
              << "\n";
  }
  ~CloggerSection() {
    __clogger << std::setfill(JASSTR('-')) << std::setw(30) << '\n';
  }
};

struct CLoggerTimerSection : public CloggerSection {
  using ClockType = std::chrono::system_clock;
  CLoggerTimerSection(const String& sectionName)
      : CloggerSection(sectionName) {}

  ~CLoggerTimerSection() {
    using namespace std::chrono;
    __clogger
        << "Elapse time = "
        << duration_cast<microseconds>(ClockType::now() - startTime).count()
        << "us\n";
  }

  ClockType::time_point startTime = ClockType::now();
};
}  // namespace jas
