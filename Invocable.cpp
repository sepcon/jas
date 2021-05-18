#include "Invocable.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <functional>
#include <map>
namespace jas {

struct Version;
template <class _transformer>
String transform(String s, _transformer&& tr);
static time_t current_time();
static String tolower(const String& input);
static String toupper(const String& input);
static int cmp_ver(const JsonTrait::Array& input);

static const FunctionsMap& _funcs_map() {
  static FunctionsMap _ = {
      {"current_time", __ni(current_time)},
      {"tolower", tolower},
      {"toupper", toupper},
      {"cmp_ver", cmp_ver},
  };
  return _;
}

struct Version {
  using Numbers = std::vector<String>;
  Version(const String& sver) : numbers{decomposite(sver)} {}
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

  static Numbers decomposite(const String& sver) {
    IStringStream iss(sver);
    String number;
    Numbers numbers;
    while (std::getline(iss, number, '.')) {
      numbers.push_back(std::move(number));
    }
    return numbers;
  }

  Numbers numbers;
};

DirectVal invokeFunction(const FunctionsMap& funcs_map, const String& func_name,
                         const DirectVal& e) {
  auto it = funcs_map.find(func_name);
  throwIf<function_not_found_error>(
      it == funcs_map.end(), strJoin("Unkown function `", func_name, "`!"));
  return it->second(e);
}

time_t current_time() { return std::time(nullptr); }

template <class _transformer>
String transform(String s, _transformer&& tr) {
  std::transform(std::begin(s), std::end(s), std::begin(s), tr);
  return s;
}

static String tolower(const String& input) {
  return transform(input, std::tolower);
}

static String toupper(const String& input) {
  return transform(input, std::toupper);
}

static int cmp_ver(const JsonTrait::Array& input) {
  throwIf<EvaluationError>(
      JsonTrait::size(input) != 2 || !JsonTrait::isString(input[0]) ||
          !JsonTrait::isString(input[1]),
      strJoin(
          R"(function `cmp_ver`: invalid argument type > [expect] `["first", "second"]` > [observed]: `)",
          JsonTrait::dump(input), "`"));

  return Version{JsonTrait::get<String>(input[0])}.cmp(
      JsonTrait::get<String>(input[1]));
}

DirectVal globalInvoke(const String& funcName, const DirectVal& e) {
  return invokeFunction(_funcs_map(), funcName, e);
}

void errorNotImplemented(const String& funcName) {
  throw_<function_not_implemented_error>(funcName);
}

}  // namespace jas
