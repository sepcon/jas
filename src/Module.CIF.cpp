#include "jas/Module.CIF.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>

#include "jas/FunctionModuleBaseT.h"
#include "jas/SyntaxEvaluatorImpl.h"

namespace jas {
namespace mdl {
namespace cif {
using namespace std;
struct Version;
using JasUtilityFunction = std::function<Var(const Var&)>;

template <class _transformer>
Var transform(const Var& s, _transformer&& tr);

/// No-Input function: a mask for ignoring input
template <class _callable>
inline JasUtilityFunction __ni(_callable&& f) {
  return [f = std::move(f)](const Var&) { return f(); };
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

Var current_time() { return std::time(nullptr); }
Var current_time_diff(const Var& timepoint) {
  __jas_func_throw_invalidargs_if(!timepoint.isNumber(), "Expect an integer",
                                  timepoint);
  return static_cast<int32_t>(std::time(nullptr) - timepoint.getInt());
}

template <class _transformer>
Var transform(const Var& s, _transformer&& tr) {
  __jas_func_throw_invalidargs_if(!s.isString(), "Expect a string", s);
  auto out = s.getString();
  std::transform(std::begin(s.asString()), std::end(s.asString()),
                 std::begin(out), tr);
  return out;
}

Var tolower(const Var& input) { return transform(input, ::tolower); }

Var toupper(const Var& input) { return transform(input, ::toupper); }

int _cmpVer(const Var& input) {
  __jas_func_throw_invalidargs_if(
      !input.isList() || input.size() != 2 || !input.at(0).isString() ||
          !input.at(1).isString(),
      R"(invalid argument type > [expect] `["str_first", "str_second"]`)",
      input);
  return Version{input[0].asString()}.cmp(input[1].asString());
}
Var cmp_ver(const Var& input) { return _cmpVer(input); }
Var eq_ver(const Var& input) { return _cmpVer(input) == 0; }
Var ne_ver(const Var& input) { return _cmpVer(input) != 0; }
Var lt_ver(const Var& input) { return _cmpVer(input) < 0; }
Var gt_ver(const Var& input) { return _cmpVer(input) > 0; }
Var ge_ver(const Var& input) { return _cmpVer(input) >= 0; }
Var le_ver(const Var& input) { return _cmpVer(input) <= 0; }

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

static bool matchVer(const Version& ver, const Var& pattern);

static bool matchArrayVers(const Version& ver, const Var& listPatterns) {
  assert(listPatterns.isList());
  for (auto& pattern : listPatterns.asList()) {
    if (matchVer(ver, pattern)) {
      return true;
    }
  }
  return false;
}
static bool matchVer(const Version& ver, const Var& pattern) {
  if (pattern.isString()) {
    return matchVer(ver, Version{pattern.asString()});
  } else if (pattern.isList()) {
    return matchArrayVers(ver, pattern);
  } else {
    return false;
  }
}

Var match_ver(const Var& input) {
  __jas_ni_func_throw_invalidargs_if(
      !input.isList() || input.size() < 2,
      R"(input must be array that satisfies: ["version", ["pattern1", ..."patternN"], or ["version", "pattern1", .... "patternN"])");

  __jas_func_throw_invalidargs_if(!input[0].isString(),
                                  "input version must be a string", input[0]);

  Version ver = input[0].asString();
  for (size_t i = 1; i < input.size(); ++i) {
    if (matchVer(ver, input[i])) {
      return true;
    }
  }
  return false;
}

Var contains(const Var& input) {
  __jas_func_throw_invalidargs_if(!input.isList() || input.size() != 2,
                                  R"(expect: ["first", "second"])", input);

  if (input[0].isString()) {
    return input[0].asString().find(input[1].asString()) <
           input[0].asString().size();
  } else if (input[0].isList()) {
    auto& arr = input[0].asList();
    return find(begin(arr), end(arr), input[1]) != end(arr);
  } else if (input[0].isDict()) {
    __jas_func_throw_invalidargs_if(!input[1].isString(),
                                    "expect: second arg is string`", input);
    return input[0].contains(input[1].asString());
  } else {
    return false;
  }
}

Var to_string(const Var& input) { return input.dump(); }

time_t _unix_timestamp(const String& strDateTime, const CharType* format) {
  IStringStream iss(strDateTime);
  std::tm t = {};
  iss >> std::get_time(&t, format);
  return std::mktime(&t);
}

Var unix_timestamp(const Var& input) {
  if (input.isString()) {
    auto& strDateTime = input.asString();
    return strDateTime.empty()
               ? -1
               : _unix_timestamp(strDateTime, JASSTR("%Y/%m/%d %H:%M:%S"));
  } else if (input.isList()) {
    auto& arr = input.asList();
    __jas_func_throw_invalidargs_if(
        arr.size() != 2 || !(input[0].isString() && input[1].isString()),
        "input must be array of 2 strings", input);
    return _unix_timestamp(input[0].asString(), input[1].asString().c_str());
  } else {
    return -1;
  }
}

Var has_null_val(const Var& input) {
  if (input.isNull()) {
    return true;
  } else if (input.isDict()) {
    for (auto& [key, val] : input.asDict()) {
      if (val.isNull()) {
        return true;
      }
    }
  } else if (input.isList()) {
    for (auto& item : input.asList()) {
      if (item.isNull()) {
        return true;
      }
    }
  }
  return false;
}

Var len(const Var& input) {
  if (input.isString()) {
    return input.asString().size();
  } else {
    __jas_func_throw_invalidargs_if(
        !(input.isList() || input.isDict()),
        JASSTR("input must be array or object or string"), input);
    return input.size();
  }
}

Var is_even(const Var& integer) {
  __jas_func_throw_invalidargs_if(!integer.isNumber(), "expected an integer",
                                  integer);
  return integer.getInt() % 2 == 0;
}

Var is_odd(const Var& integer) {
  __jas_func_throw_invalidargs_if(!integer.isNumber(), "expected an integer",
                                  integer);
  return integer.getInt() % 2 == 1;
}

bool _empty(const Var& data) {
  if (data.isList() || data.isDict()) {
    return data.size() == 0;
  } else if (data.isNull()) {
    return true;
  } else if (data.isString()) {
    return data.asString().size() == 0;
  }
  return false;
}
Var empty(const Var& data) { return _empty(data); }

Var not_empty(const Var& data) { return !_empty(data); }

Var abs(const Var& data) {
  __jas_func_throw_invalidargs_if(!(data.isNumber()), "accept only number type",
                                  data);
  if (data.isDouble()) {
    return std::abs(data.getDouble());
  } else {
    return std::abs(data.getInt());
  }
}

Var range(const Var& data) {
  __jas_func_throw_invalidargs_if(
      !data.isList() || data.size() > 3 || data.size() == 0,
      "expect a list as [begin, end, (step)]", data);
  auto& options = data.asList();
  auto& vstart = options.front();
  auto& vend = options[1];
  __jas_func_throw_invalidargs_if(!vstart.isNumber() || !vend.isNumber(),
                                  "Expect a list of numbers", data);

  Number start = vstart;
  Number end = vend;
  Number step = start < end ? 1 : -1;
  if (options.size() == 3) {
    __jas_func_throw_invalidargs_if(!options[2].isInt(),
                                    "Expect step is a number", options[2]);
    step = options[2];
  }

  __jas_func_throw_invalidargs_if(step == Number{0}, "Step must not be Zero",
                                  data);
  Var::List range;
  if (start < end) {
    if (step > Number{0}) {
      do {
        range.emplace_back(start);
      } while ((start += step) < end);
    } else {
      range.emplace_back(start);
      while (start < (end += step)) {
        range.emplace_back(end);
      }
    }
  } else {
    if (step > Number{0}) {
      do {
        range.emplace_back(start);
      } while ((start -= step) > end);
    } else {
      range.emplace_back(start);
      while (start > (end -= step)) {
        range.emplace_back(end);
      }
    }
  }
  return range;
}

Var cdebug(const Var& data) {
  std::cout << "[DEBUG]: " << data.dump().c_str() << std::endl;
  return {};
}

class CIFModule : public FunctionModuleBaseT<JasUtilityFunction> {
 public:
  String moduleName() const override { return {}; }
  const FunctionsMap& _funcMap() const override {
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
        {JASSTR("range"), range},
        {JASSTR("cdebug"), cdebug},
    };
    return _;
  }
  Var invoke(const JasUtilityFunction& func, EvaluablePtr param,
             SyntaxEvaluatorImpl* evaluator) override {
    return func(evaluator->evalAndReturn(param.get()));
  }
};

std::shared_ptr<FunctionModuleIF> getModule() {
  static auto module = make_shared<CIFModule>();
  return module;
}

}  // namespace cif
}  // namespace mdl
}  // namespace jas
