#pragma once

#include <set>

#include "String.h"

namespace jas {
namespace supported_func {

inline constexpr auto cmp_ver = "cmp_ver";
inline constexpr auto prop = "prop";
inline constexpr auto evchg = "evchg";
inline constexpr auto tolower = "tolower";
inline constexpr auto toupper = "toupper";
inline constexpr auto current_time = "current_time";
inline constexpr auto snchg = "snchg";

inline bool allow(const StringView& funcname) {
  static const std::set<StringView> _supportedFuncs{
      cmp_ver, prop, evchg, tolower, toupper, current_time, snchg};
  return _supportedFuncs.find(funcname) != _supportedFuncs.end();
}
}  // namespace supported_func
}  // namespace jas
