#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>

#include "Json.h"
#include "String.h"

namespace jas {

#ifdef JAS_USE_WSTR
#define __jascout std::wcout
#define __jascerr std::wcerr
#else
#define __jascout std::cout
#define __jascerr std::cerr
#endif

#define __jas_clogger_class(name, impl)               \
  struct __##name {                                   \
    template <typename T>                             \
    const __##name& operator<<(const T& v) const {    \
      impl << v;                                      \
      return *this;                                   \
    }                                                 \
                                                      \
    const __##name& operator<<(const Json& e) const { \
      impl << JsonTrait::dump(e);                     \
      return *this;                                   \
    }                                                 \
                                                      \
    ~__##name() { impl << std::endl; /*"\n";*/ }      \
  };                                                  \
                                                      \
  inline auto name() { return __##name{}; }

__jas_clogger_class(cloginfo, __jascout);
__jas_clogger_class(clogerr, __jascerr);

#undef __jas_clogger_class

struct CloggerSection {
  size_t titleLen = 0;
  CloggerSection(const String& sectionName) {
    titleLen = sectionName.size() + 8;
    __jascout << "==> [" << sectionName << "]:\n"
              << std::setfill(JASSTR('-')) << std::setw(titleLen) << "\n";
  }
  ~CloggerSection() {
    __jascout << std::setfill(JASSTR('-')) << std::setw(titleLen) << "\n";
  }
};

struct CLoggerTimerSection : public CloggerSection {
  using ClockType = std::chrono::system_clock;
  CLoggerTimerSection(const String& sectionName)
      : CloggerSection(sectionName) {}

  ~CLoggerTimerSection() {
    using namespace std::chrono;
    __jascout
        << "Elapse time = "
        << duration_cast<microseconds>(ClockType::now() - startTime).count()
        << "us\n";
  }

  ClockType::time_point startTime = ClockType::now();
};
}  // namespace jas
