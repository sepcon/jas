#include "Parser.h"

#include <map>
#include <optional>
#include <vector>

#include "EvaluableClasses.h"
#include "Exception.h"
#include "Keywords.h"
#include "SupportedFunctions.h"

namespace jas {
namespace parser {

using ObjectType = typename JsonTrait::Object;
using ArrayType = typename JsonTrait::Array;
using ParsingRuleCallback = EvaluablePtr (*)(const Json&);
using KeywordParserMap = std::map<String, ParsingRuleCallback>;
struct EvaluableInfo {
  String type;
  Json params;
  String id;
};

template <typename T>
T jsonGet(const Json& j);
template <typename T>
T jsonGet(const Json& j, const String& key);
template <typename T>
T jsonGet(const Json& j, const String& key);
bool jsonHasKey(const Json& j, const String& key);
Json jsonGet(const Json& j, const String& key);

static EvaluablePtr parseInvalidData(const Json& j);
static EvaluablePtr parseNoneKeywordVal(const Json& j);
static std::optional<EvaluableInfo> extractOperationInfo(const Json& j);
static EvaluablePtr parseFunction(const Json& j);
static EvaluablePtr parseContextVal(const Json& j);
static EvaluablePtr parseProperty(const Json& j);
static EvaluablePtr parseShortcutFunctionCall(const Json& j);
static std::vector<ParsingRuleCallback>& parsers();

template <typename T>
T jsonGet(const Json& j) {
  return JsonTrait::template get<T>(j);
}
template <typename T>
T jsonGet(const Json& j, const String& key) {
  return parser::jsonGet<T>(jsonGet(j, key));
}

Json jsonGet(const Json& j, const String& key) {
  return JsonTrait::get(j, key);
}
bool jsonHasKey(const Json& j, const String& key) {
  return JsonTrait::hasKey(j, key);
}

//============================= PARSERS ===============================
static EvaluablePtr parseInvalidData(const Json& j) {
  throw_<SyntaxError>("Not an Evaluable: ", JsonTrait::dump(j));
  return {};
}

static EvaluablePtr parseNoneKeywordVal(const Json& j) {
  EvaluablePtr evb;
  if (JsonTrait::isInt(j)) {
    evb = makeConst(jsonGet<int64_t>(j));
  } else if (JsonTrait::isDouble(j)) {
    evb = makeConst(jsonGet<double>(j));
  } else if (JsonTrait::isBool(j)) {
    evb = makeConst(jsonGet<bool>(j));
  } else if (JsonTrait::isString(j)) {
    evb = makeConst(jsonGet<String>(j));
  } else if (JsonTrait::isArray(j)) {
    auto ea = makeEArray();
    for (auto& je : JsonTrait::get<ArrayType>(j)) {
      ea->value.emplace_back(parser::parse(je));
    }
    evb = ea;
  } else if (JsonTrait::isObject(j)) {
    auto em = makeEMap();
    for (auto& [key, je] : JsonTrait::get<ObjectType>(j)) {
      em->value.emplace(key, parser::parse(je));
    }
    evb = std::move(em);
  }
  return evb;
}

static std::optional<EvaluableInfo> extractOperationInfo(const Json& j) {
  if (JsonTrait::isObject(j)) {
    auto oj = jsonGet<ObjectType>(j);
    for (auto& [key, val] : oj) {
      if (!key.empty() && key[0] == '@') {
        return EvaluableInfo{key, val, jsonGet<String>(j, keyword::id)};
      }
    }
  }
  return {};
}

template <class _op_parser, class _operation_type>
struct OperationParserBase {
  using operation_type = _operation_type;
  static operation_type mapToEvbType(const String& sop) {
    if (auto it = _op_parser::s2op_map().find(sop);
        it != std::end(_op_parser::s2op_map())) {
      return it->second;
    } else {
      return operation_type::invalid;
    }
  }
};

template <class _op_parser, class _operation_type>
struct OperatorParserBase
    : public OperationParserBase<_op_parser, _operation_type> {
  using _Base = OperationParserBase<_op_parser, _operation_type>;
  static EvaluablePtr parse(const Json& j) {
    if (auto extracted = extractOperationInfo(j)) {
      auto& [sop, jevaluation, id] = extracted.value();
      if (auto op = _Base::mapToEvbType(sop); op != _operation_type::invalid) {
        if (JsonTrait::isArray(jevaluation)) {
          Evaluables params;
          for (auto& jparam : jsonGet<ArrayType>(jevaluation)) {
            params.emplace_back(parser::parse(jparam));
          }
          return makeOp(std::move(id), op, std::move(params));
        }
      }
    }
    return {};
  }
};

struct ArithmeticalOpParser
    : public OperatorParserBase<ArithmeticalOpParser, ArithmeticOperatorType> {
  static const std::map<String, ArithmeticOperatorType>& s2op_map() {
    static std::map<String, ArithmeticOperatorType> _ = {
        {keyword::plus, ArithmeticOperatorType::plus},
        {keyword::minus, ArithmeticOperatorType::minus},
        {keyword::multiplies, ArithmeticOperatorType::multiplies},
        {keyword::divides, ArithmeticOperatorType::divides},
        {keyword::modulus, ArithmeticOperatorType::modulus},
        {keyword::bit_and, ArithmeticOperatorType::bit_and},
        {keyword::bit_or, ArithmeticOperatorType::bit_or},
        {keyword::bit_not, ArithmeticOperatorType::bit_not},
        {keyword::bit_xor, ArithmeticOperatorType::bit_xor},
        {keyword::negate, ArithmeticOperatorType::negate},
    };
    return _;
  }
};
struct LogicalOpParser
    : public OperatorParserBase<LogicalOpParser, LogicalOperatorType> {
  static const std::map<String, LogicalOperatorType>& s2op_map() {
    static std::map<String, LogicalOperatorType> _ = {
        {keyword::logical_and, LogicalOperatorType::logical_and},
        {keyword::logical_or, LogicalOperatorType::logical_or},
        {keyword::logical_not, LogicalOperatorType::logical_not},
    };
    return _;
  }
};
struct ComparisonOpParser
    : public OperatorParserBase<ComparisonOpParser, ComparisonOperatorType> {
  static const std::map<String, ComparisonOperatorType>& s2op_map() {
    static std::map<String, ComparisonOperatorType> _ = {
        {keyword::eq, ComparisonOperatorType::eq},
        {keyword::neq, ComparisonOperatorType::neq},
        {keyword::lt, ComparisonOperatorType::lt},
        {keyword::gt, ComparisonOperatorType::gt},
        {keyword::le, ComparisonOperatorType::le},
        {keyword::ge, ComparisonOperatorType::ge},
    };
    return _;
  }
};

struct ListOpParser
    : public OperationParserBase<ListOpParser, ListOperationType> {
  static const std::map<String, ListOperationType>& s2op_map() {
    static std::map<String, ListOperationType> _ = {
        {keyword::any_of, ListOperationType::any_of},
        {keyword::all_of, ListOperationType::all_of},
        {keyword::none_of, ListOperationType::none_of},
        {keyword::size_of, ListOperationType::size_of},
        {keyword::count_if, ListOperationType::count_if},
    };
    return _;
  }

  static EvaluablePtr parse(const Json& j) {
    EvaluablePtr output;
    do {
      auto extracted = extractOperationInfo(j);
      if (!extracted) {
        break;
      }
      auto& [sop, jevaluable, id] = extracted.value();
      auto op = mapToEvbType(sop);
      if (op == ListOperationType::invalid) {
        break;
      }

      auto throw_syntax_error = [](bool condition, const String& opname) {
        throwIf<SyntaxError>(condition, "Follow operation `", opname,
                             "` must be an evaluable syntax");
      };

      auto jcond = jsonGet(jevaluable, keyword::cond);
      if (!JsonTrait::isNull(jcond)) {
        if (auto cond = parser::parse(jsonGet(jevaluable, keyword::cond))) {
          output = makeOp(std::move(id), op, std::move(cond),
                          jsonGet<String>(jevaluable, keyword::item_id),
                          parser::parse(jsonGet(jevaluable, keyword::list)));
          break;
        }
      }

      if (JsonTrait::isString(jevaluable)) {
        // accept string only for `size_of` operation, any other op
        throw_syntax_error((op != ListOperationType::size_of), sop);
        output = makeOp(std::move(id), op, makeConst(true), {},
                        makeConst(jsonGet<String>(jevaluable)));
        break;
      } else {
        auto cond = parser::parse(jevaluable);
        throw_syntax_error(!cond, sop);
        output = makeOp(std::move(id), op, std::move(cond));
      }
    } while (false);
    return output;
  }
};

static EvaluablePtr parseFunction(const Json& j) {
  auto extracted = extractOperationInfo(j);
  if (extracted) {
    auto& [sop, jevaluable, id] = extracted.value();
    if (sop == keyword::call) {
      if (JsonTrait::isString(jevaluable)) {
        return makeFnc(std::move(id), jsonGet<String>(jevaluable));
      } else if (JsonTrait::isObject(jevaluable)) {
        auto jfunc = jsonGet(jevaluable, keyword::func);
        throwIf<SyntaxError>(!JsonTrait::isString(jfunc), "missing keyword `",
                             keyword::func, "` after operation `", sop, "`");

        auto funcName = jsonGet<String>(jfunc);
        throwIf<SyntaxError>(!supported_func::allow(funcName), "Function ",
                             funcName, " is not supported");
        return makeFnc(std::move(id), std::move(funcName),
                       parser::parse(jsonGet(jevaluable, keyword::param)));
      }
    } else if (sop == keyword::cmp_ver) {
      throwIf<SyntaxError>(
          !JsonTrait::isArray(jevaluable) || JsonTrait::size(jevaluable) != 2,
          "following keyword `", keyword::cmp_ver,
          "` must be an array of exactly 2 strings");
      return makeFnc(std::move(id), supported_func::cmp_ver,
                     parser::parse(jevaluable));
    } else if (sop == keyword::prop) {
      throwIf<SyntaxError>(!JsonTrait::isString(jevaluable),
                           "following keyword `", sop,
                           "` must be a string of property name");

      return makeFnc(std::move(id), supported_func::prop,
                     makeConst(jevaluable));
    } else if (sop == keyword::evchg) {
      return makeFnc(std::move(id), supported_func::evchg,
                     parser::parse(jevaluable));
    }
  } else if (JsonTrait::isString(j)) {
    auto str = JsonTrait::get<String>(j);
    if (str.find(keyword::call) == 0) {
      auto colonPos = str.find(":");
      if (colonPos < str.size()) {
        return makeFnc({}, str.substr(colonPos + 1));
      }
    }
  }
  return {};
}

static EvaluablePtr parseContextVal(const Json& j) {
  auto extracted = extractOperationInfo(j);
  EvaluablePtr evaluated;
  do {
    if (!extracted) {
      auto str = JsonTrait::get<String>(j);
      if (str.empty()) {
        break;
      }
      if (str.find(keyword::value_of) != 0) {
        break;
      }
      auto colonPos = str.find(":");
      if (colonPos >= str.size()) {
        break;
      }

      evaluated = makeCV({}, str.substr(colonPos + 1));
      break;
    }

    auto& [sop, jevaluable, id] = extracted.value();
    if (sop != keyword::value_of) {
      break;
    }
    if (JsonTrait::isString(jevaluable)) {
      evaluated = makeCV(std::move(id), jsonGet<String>(jevaluable));
      break;
    }

    throwIf<SyntaxError>(
        !JsonTrait::isObject(jevaluable), "ollow keyword `", keyword::value_of,
        "` must be a json object or a string - `", JsonTrait::dump(j));

    auto jpath = jsonGet(jevaluable, keyword::path);
    throwIf<SyntaxError>(!JsonTrait::isString(jpath), "Operation `",
                         keyword::value_of, "` must specify `", keyword::path,
                         "` to data item");

    auto data_path = jsonGet<String>(jpath);
    auto jsnapshot = JsonTrait::get(jevaluable, keyword::snapshot);
    int32_t snapshot_idx = ContextVal::new_;
    if (JsonTrait::isInt(jsnapshot)) {
      snapshot_idx = jsonGet<int32_t>(jsnapshot);
    }
    evaluated = makeCV(std::move(id), std::move(data_path), snapshot_idx);
  } while (false);
  return evaluated;
}
static EvaluablePtr parseProperty(const Json& j) {
  if (JsonTrait::isString(j)) {
    auto str = JsonTrait::get<String>(j);
    if (!str.empty() && str[0] == prefix::property) {
      return makeProp(std::move(str));
    }
  }
  return {};
}

static EvaluablePtr parseShortcutFunctionCall(const Json& j) {
  EvaluablePtr e;
  do {
    if (!JsonTrait::isString(j)) {
      break;
    }

    auto str = JsonTrait::get<String>(j);
    if (str.empty() || str[0] != prefix::operation) {
      break;
    }
    StringView sv = str;

    auto func = sv.substr(1);
    auto colonPos = func.find(":");
    String funcName;
    EvaluablePtr param;
    if (colonPos < func.size()) {
      funcName = func.substr(0, colonPos);
    } else {
      funcName = func;
    }

    throwIf<SyntaxError>(!supported_func::allow(funcName),
                         "Not supported function: ", funcName);
    param =
        parser::parse(JsonTrait::makeJson(String{func.substr(colonPos + 1)}));
    e = makeFnc({}, std::move(funcName), std::move(param));

  } while (false);
  return e;
}

static std::vector<ParsingRuleCallback>& parsers() {
  static std::vector<ParsingRuleCallback> _ = {
      ArithmeticalOpParser::parse,
      LogicalOpParser::parse,
      ComparisonOpParser::parse,
      ListOpParser::parse,
      parseContextVal,
      parseFunction,
      parseProperty,
      parseShortcutFunctionCall,
      parseNoneKeywordVal,  // this function must be call last
      parseInvalidData,
  };
  return _;
}

EvaluablePtr parse(const Json& j) {
  for (auto& parser : parsers()) {
    if (auto evaled = parser(j)) {
      return evaled;
    }
  }
  return {};
}

}  // namespace parser
}  // namespace jas
