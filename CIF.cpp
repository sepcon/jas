#include "CIF.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <functional>
#include <iomanip>
#include <map>

namespace jas {

/// context independent function
namespace cif {
struct Version;
template <class _transformer>
String transform(String s, _transformer&& tr);
static time_t current_time();
static int32_t current_time_diff(time_t timepoint);
static String tolower(const String& input);
static String toupper(const String& input);
static int cmp_ver(const JsonArray& input);
static bool eq_ver(const JsonArray& input);
static bool ne_ver(const JsonArray& input);
static bool lt_ver(const JsonArray& input);
static bool gt_ver(const JsonArray& input);
static bool ge_ver(const JsonArray& input);
static bool le_ver(const JsonArray& input);
static bool match_ver(const JsonArray& input);
static bool contains(const JsonArray& input);
static String to_string(const Json& input);
static time_t unix_timestamp(const Json& input);
static bool has_null_val(const Json& input);
static size_t len(const Json& input);
static bool is_even(const Json& integer);
static bool is_odd(const Json& integer);
static bool empty(const Json& data);
static bool not_empty(const Json& data);
static Json abs(const Json& data);

static const FunctionsMap& _funcsMap() {
  static FunctionsMap _ = {
      {JASSTR("current_time"), __ni(current_time)},
      {JASSTR("current_time_diff"), current_time_diff},
      {JASSTR("tolower"), tolower},
      {JASSTR("toupper"), toupper},
      {JASSTR("cmp_ver"), cmp_ver},
      {JASSTR("eq_ver"), eq_ver},
      {JASSTR("ne_ver"), ne_ver},
      {JASSTR("lt_ver"), lt_ver},
      {JASSTR("gt_ver"), gt_ver},
      {JASSTR("ge_ver"), ge_ver},
      {JASSTR("le_ver"), le_ver},
      {JASSTR("match_ver"), match_ver},
      {JASSTR("contains"), contains},
      {JASSTR("to_string"), to_string},
      {JASSTR("unix_timestamp"), unix_timestamp},
      {JASSTR("has_null_val"), has_null_val},
      {JASSTR("len"), len},
      {JASSTR("is_even"), is_even},
      {JASSTR("is_odd"), is_odd},
      {JASSTR("empty"), empty},
      {JASSTR("not_empty"), not_empty},
      {JASSTR("abs"), abs},
  };
  return _;
}

struct Version {
  using Numbers = std::vector<String>;
  Version(const String& sver) : numbers{decompose(sver)} {}
  int cmp(const Version& other) const {
    if (numbers.size() == other.numbers.size()) {
      for (size_t i = 0; i < numbers.size(); ++i) {
        try {
          auto num = std::stol(numbers[i]);
          auto onum = std::stol(other.numbers[i]);
          if (num == onum) {
            continue;
          } else {
            return num < onum ? -1 : 1;
          }
        } catch (const std::exception&) {
          if (numbers[i] == other.numbers[i]) {
            continue;
          } else {
            return numbers[i] < other.numbers[i] ? -1 : 1;
          }
        }
      }
      return 0;
    } else {
      return numbers.size() < other.numbers.size() ? -1 : 1;
    }
  }

  static Numbers decompose(const String& sver) {
    IStringStream iss(sver);
    String number;
    Numbers numbers;
    while (std::getline(iss, number, JASSTR('.'))) {
      numbers.push_back(std::move(number));
    }
    return numbers;
  }

  Numbers numbers;
};

time_t current_time() { return std::time(nullptr); }
int32_t current_time_diff(time_t timepoint) {
  return static_cast<int32_t>(std::time(nullptr) - timepoint);
}

template <class _transformer>
String transform(String s, _transformer&& tr) {
  std::transform(std::begin(s), std::end(s), std::begin(s), tr);
  return s;
}

static String tolower(const String& input) {
  return transform(input, ::tolower);
}

static String toupper(const String& input) {
  return transform(input, ::toupper);
}

static int cmp_ver(const JsonArray& input) {
  __jas_func_throw_invalidargs_if(
      JsonTrait::size(input) != 2 || !JsonTrait::isString(input[0]) ||
          !JsonTrait::isString(input[1]),
      R"(invalid argument type > [expect] `["str_first", "str_second"]`)",
      input);

  return Version{JsonTrait::get<String>(input[0])}.cmp(
      JsonTrait::get<String>(input[1]));
}
static bool eq_ver(const JsonArray& input) { return cmp_ver(input) == 0; }
static bool ne_ver(const JsonArray& input) { return cmp_ver(input) != 0; }
static bool lt_ver(const JsonArray& input) { return cmp_ver(input) < 0; }
static bool gt_ver(const JsonArray& input) { return cmp_ver(input) > 0; }
static bool ge_ver(const JsonArray& input) { return cmp_ver(input) >= 0; }
static bool le_ver(const JsonArray& input) { return cmp_ver(input) <= 0; }

static bool matchVer(const Version& ver, const Version& pattern) {
  if (pattern.numbers.empty()) {
    return true;
  }
  if (ver.numbers.empty()) {
    return false;
  }
  auto minNumber = std::min(ver.numbers.size(), pattern.numbers.size());
  size_t i = 0;
  for (; i < minNumber; ++i) {
    if (ver.numbers[i] != pattern.numbers[i]) {
      break;
    }
  }
  return i == minNumber ||
         (i == minNumber - 1 && pattern.numbers[i] == JASSTR("x"));
}

static bool matchVer(const Version& ver, const Json& pattern);

static bool matchVer(const Version& ver, const JsonArray& listPatterns) {
  for (auto& pattern : listPatterns) {
    if (matchVer(ver, pattern)) {
      return true;
    }
  }
  return false;
}
static bool matchVer(const Version& ver, const Json& pattern) {
  if (JsonTrait::isString(pattern)) {
    return matchVer(ver, Version{JsonTrait::get<String>(pattern)});
  } else if (JsonTrait::isArray(pattern)) {
    return matchVer(ver, JsonTrait::get<JsonArray>(pattern));
  } else {
    return false;
  }
}
static bool match_ver(const JsonArray& input) {
  __jas_ni_func_throw_invalidargs_if(
      input.size() < 2,
      R"(input must satisfy: ["version", ["pattern1", ..."patternN"], or ["version", "pattern1", .... "patternN"])");

  __jas_func_throw_invalidargs_if(!JsonTrait::isString(input[0]),
                                  "input version must be a string", input[0]);

  Version ver = JsonTrait::get<String>(input[0]);

  size_t i = 1;
  for (; i < JsonTrait::size(input); ++i) {
    if (matchVer(ver, input[i])) {
      return true;
    }
  }
  return false;
}

static bool contains(const JsonArray& input) {
  __jas_func_throw_invalidargs_if(
      !JsonTrait::isArray(input) || JsonTrait::size(input) != 2,
      R"(expect: ["first", "second"])", input);

  if (JsonTrait::isString(input[0])) {
    auto first = JsonTrait::get<String>(input[0]);
    return first.find(JsonTrait::get<String>(input[1])) < first.size();
  } else if (JsonTrait::isArray(input[0])) {
    decltype(auto) arr = JsonTrait::get<JsonArray>(input[0]);
    return std::find(std::begin(arr), std::end(arr), input[1]) != std::end(arr);
  } else if (JsonTrait::isObject(input[0])) {
    __jas_func_throw_invalidargs_if(!JsonTrait::isString(input[1]),
                                    "expect: second arg is string`", input);
    return JsonTrait::hasKey(input[0], JsonTrait::get<String>(input[1]));
  } else {
    return false;
  }
}

static String to_string(const Json& input) { return JsonTrait::dump(input); }

static time_t _unix_timestamp(const String& strDateTime,
                              const CharType* format) {
  IStringStream iss(strDateTime);
  std::tm t = {};
  iss >> std::get_time(&t, format);
  return std::mktime(&t);
}

static time_t unix_timestamp(const Json& input) {
  if (JsonTrait::isString(input)) {
    auto strDateTime = JsonTrait::get<String>(input);
    __jas_ni_func_throw_invalidargs_if(
        strDateTime.empty(),
        "input of `unix_timestamp` must be string of "
        "format [%Y/%m/%d %H:%M:%S]");
    return _unix_timestamp(strDateTime, JASSTR("%Y/%m/%d %H:%M:%S"));
  } else if (JsonTrait::isArray(input)) {
    decltype(auto) arr = JsonTrait::get<JsonArray>(input);
    __jas_func_throw_invalidargs_if(
        arr.size() != 2 ||
            !(JsonTrait::isString(arr[0]) && JsonTrait::isString(arr[1])),
        "input must be array of 2 strings", input);
    return _unix_timestamp(JsonTrait::get<String>(arr[0]),
                           JsonTrait::get<String>(arr[1]).c_str());
  } else {
    __jas_ni_func_throw_invalidargs("Unrecognized argument");
    return -1;
  }
}

static bool has_null_val(const Json& input) {
  if (JsonTrait::isNull(input)) {
    return true;
  }
  if (JsonTrait::isObject(input)) {
    for (auto& [key, val] : JsonTrait::get<JsonObject>(input)) {
      if (JsonTrait::isNull(val)) {
        return true;
      }
    }
    return false;
  } else if (JsonTrait::isArray(input)) {
    for (auto& item : JsonTrait::get<JsonArray>(input)) {
      if (JsonTrait::isNull(item)) {
        return true;
      }
    }
    return false;
  }
  return false;
}

static size_t len(const Json& input) {
  if (JsonTrait::isString(input)) {
    return JsonTrait::get<String>(input).size();
  } else {
    __jas_func_throw_invalidargs_if(
        !(JsonTrait::isArray(input) || JsonTrait::isObject(input)),
        JASSTR("input must be array or object"), input);
    return JsonTrait::size(input);
  }
}

static bool is_even(const Json& integer) {
  __jas_func_throw_invalidargs_if(!JsonTrait::isInt(integer),
                                  "expected an integer", integer);
  return JsonTrait::get<int>(integer) % 2 == 0;
}

static bool is_odd(const Json& integer) { return !is_even(integer); }

static bool empty(const Json& data) {
  if (JsonTrait::isArray(data) || JsonTrait::isObject(data)) {
    return JsonTrait::size(data) == 0;
  } else {
    __jas_func_throw_invalidargs("applies for array/object only", data);
  }
  return true;
}

static bool not_empty(const Json& data) { return !empty(data); }
static Json abs(const Json& data) {
  __jas_func_throw_invalidargs_if(
      !(JsonTrait::isDouble(data) || JsonTrait::isInt(data)),
      "accept only number type", data);
  if (JsonTrait::isDouble(data)) {
    return std::abs(JsonTrait::get<double>(data));
  } else {
    return std::abs(JsonTrait::get<int>(data));
  }
}

JsonAdapter invoke(const String& funcName, const JsonAdapter& e) {
  return invoke(_funcsMap(), funcName, e);
}

bool supported(const String& funcName) {
  return _funcsMap().find(funcName) != _funcsMap().end();
}

std::vector<String> supportedFunctions() {
  std::vector<String> funcs;
  std::transform(begin(_funcsMap()), std::end(_funcsMap()),
                 std::back_insert_iterator(funcs),
                 [](auto&& p) { return p.first; });
  return funcs;
}

}  // namespace cif

JsonAdapter invoke(const FunctionsMap& funcsMap, const String& funcName,
                   const JsonAdapter& e) {
  auto it = funcsMap.find(funcName);
  throwIf<FunctionNotFoundError>(!(it != funcsMap.end()),
                                 JASSTR("Unkown function `"), funcName,
                                 JASSTR("`!"));
  try {
    return it->second(e);
  } catch (const JsonAdapter::TypeError& e) {
    throw_<FunctionInvalidArgTypeError>(
        JASSTR("Failed invoking function `"), funcName,
        JASSTR("` due to invalid argument type: "), e.what());
    return {};
  }
}

}  // namespace jas
