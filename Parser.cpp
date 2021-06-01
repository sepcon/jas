#include "Parser.h"

#include <iostream>
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

static bool isSpecifierLike(const StringView& expression);
static bool isSpecifier(const StringView& expression);
static EvaluablePtr parseInvalidData(const Json& j);
static EvaluablePtr parseNoneKeywordVal(const Json& j);
static std::optional<EvaluableInfo> extractOperationInfo(const Json& j);
static EvaluablePtr parseFunction(const Json& j);
static EvaluablePtr parseContextVal(const Json& j);
static EvaluablePtr parseProperty(const Json& j);
static Json reconstructJAS(const Json& j);
static Json constructJAS(const StringView& expression);
static Json constructJAS(const StringView& specifier,
                         const StringView& expression);
static Json constructJAS(const StringView& expression, const Json& jevaluable);
static std::vector<ParsingRuleCallback>& parsers();
static EvaluablePtr parseImpl(const Json& j);

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

template <typename T>
T jsonGet(const Json& j, const StringView& key) {
  return parser::jsonGet<T>(jsonGet(j, String{key}));
}

Json jsonGet(const Json& j, const StringView& key) {
  return JsonTrait::get(j, String{key});
}
bool jsonHasKey(const Json& j, const StringView& key) {
  return JsonTrait::hasKey(j, String{key});
}

Json makeJson(const StringView& key, Json value) {
  ObjectType o;
  JsonTrait::add(o, String{key}, std::move(value));
  return JsonTrait::makeJson(std::move(o));
}

//============================= PARSERS ===============================
static bool isSpecifierLike(const StringView& expression) {
  return !expression.empty() && expression[0] == prefix::specifier;
}
static bool isSpecifier(const StringView& kw) {
  static std::set<StringView> specifiers = {
      keyword::bit_and,
      keyword::bit_not,
      keyword::bit_or,
      keyword::bit_xor,
      keyword::divides,
      keyword::minus,
      keyword::modulus,
      keyword::multiplies,
      keyword::negate,
      keyword::plus,
      // logical operators
      keyword::logical_and,
      keyword::logical_not,
      keyword::logical_or,
      // comparison operators
      keyword::eq,
      keyword::ge,
      keyword::gt,
      keyword::le,
      keyword::lt,
      keyword::neq,
      // list operations
      keyword::all_of,
      keyword::any_of,
      keyword::none_of,
      keyword::size_of,
      keyword::count_if,
      // short functions
      keyword::cmp_ver,
      keyword::prop,
      keyword::evchg,
      // function specifier
      keyword::call,
      // context value specifier
      keyword::value_of,
      keyword::func,
      keyword::snapshot,
      keyword::param,
      keyword::cond,
      keyword::item_id,
      keyword::list,
      keyword::path,
  };

  return !kw.empty() && (specifiers.find(kw) != specifiers.end() ||
                         supported_func::allow(kw.substr(1)));
}

static EvaluablePtr parseInvalidData(const Json&) { return {}; }

static EvaluablePtr parseNoneKeywordVal(const Json& j) {
  EvaluablePtr evb;
  if (JsonTrait::isInt(j)) {
    evb = makeConst(jsonGet<int64_t>(j));
  } else if (JsonTrait::isDouble(j)) {
    evb = makeConst(jsonGet<double>(j));
  } else if (JsonTrait::isBool(j)) {
    evb = makeConst(jsonGet<bool>(j));
  } else if (JsonTrait::isString(j)) {
    auto s = jsonGet<String>(j);
    throwIf<SyntaxError>(
        isSpecifierLike(s), "A string starts with character `",
        prefix::specifier,
        "` must be an allowed keyword, or a supported function name ??", s,
        "??");
    evb = makeConst(jsonGet<String>(j));
  }
  return evb;
}

static std::optional<EvaluableInfo> extractOperationInfo(const Json& j) {
  if (JsonTrait::isObject(j)) {
    auto oj = jsonGet<ObjectType>(j);
    for (auto& [key, val] : oj) {
      if (!key.empty() && key[0] == '@') {
        return EvaluableInfo{key, val, jsonGet<String>(j, String{keyword::id})};
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
            params.emplace_back(parser::parseImpl(jparam));
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
  static const std::map<StringView, ArithmeticOperatorType>& s2op_map() {
    static std::map<StringView, ArithmeticOperatorType> _ = {
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
  static const std::map<StringView, LogicalOperatorType>& s2op_map() {
    static std::map<StringView, LogicalOperatorType> _ = {
        {keyword::logical_and, LogicalOperatorType::logical_and},
        {keyword::logical_or, LogicalOperatorType::logical_or},
        {keyword::logical_not, LogicalOperatorType::logical_not},
    };
    return _;
  }
};
struct ComparisonOpParser
    : public OperatorParserBase<ComparisonOpParser, ComparisonOperatorType> {
  static const std::map<StringView, ComparisonOperatorType>& s2op_map() {
    static std::map<StringView, ComparisonOperatorType> _ = {
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
  static const std::map<StringView, ListOperationType>& s2op_map() {
    static std::map<StringView, ListOperationType> _ = {
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

      EvaluablePtr evbCond;
      EvaluablePtr list;

      if (JsonTrait::isObject(jevaluable)) {
        auto jcond = jsonGet(jevaluable, keyword::cond);
        if (!JsonTrait::isNull(jcond)) {
          evbCond = parser::parseImpl(jcond);
        } else {
          evbCond = parser::parseImpl(jevaluable);
        }
        throwIf<SyntaxError>(
            !evbCond, "Follow operation `", sop,
            "` must be an evaluable condition with specifier `", keyword::cond,
            "`");

        output = makeOp(id, op, std::move(evbCond),
                        jsonGet<String>(jevaluable, keyword::item_id),
                        parser::parseImpl(jsonGet(jevaluable, keyword::list)));
        break;
      }

      if (JsonTrait::isString(jevaluable)) {
        // accept string only for `size_of` operation, any other op
        throw_syntax_error((op != ListOperationType::size_of), sop);
        evbCond = makeConst(true);
        output = makeOp(id, op, makeConst(true), {}, makeConst(jevaluable));
      } else if (JsonTrait::isBool(jevaluable)) {
        output = makeOp(id, op, makeConst(jevaluable), {});
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
                       parser::parseImpl(jsonGet(jevaluable, keyword::param)));
      }
    } else if (isSpecifierLike(sop)) {
      auto funcName = StringView{sop}.substr(1);
      if (supported_func::allow(funcName)) {
        return makeFnc(std::move(id), String{funcName},
                       parser::parseImpl(jevaluable));
      }
    }
  } else if (JsonTrait::isString(j)) {
    auto str = JsonTrait::get<String>(j);
    if (isSpecifierLike(str)) {
      auto funcName = StringView{str}.substr(1);
      if (supported_func::allow(funcName)) {
        return makeFnc({}, String{funcName});
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
      break;
    }

    auto& [sop, jevaluable, id] = extracted.value();
    if (sop != keyword::value_of) {
      break;
    }

    EvaluablePtr evbPath;

    if (JsonTrait::isString(jevaluable)) {
      evbPath = parser::parseImpl(jevaluable);
    } else if (JsonTrait::isObject(jevaluable)) {
      if (auto jpath = jsonGet(jevaluable, keyword::path);
          JsonTrait::isNull(jpath)) {
        evbPath = parser::parseImpl(jevaluable);
      } else {
        evbPath = parser::parseImpl(jpath);
      }
    }

    throwIf<SyntaxError>(!evbPath, "follow keyword `", sop,
                         "` must be a json object or a string: ??",
                         JsonTrait::dump(j), "??");

    auto jsnapshot = jsonGet(jevaluable, keyword::snapshot);
    int32_t snapshot_idx = ContextVal::new_;
    if (JsonTrait::isInt(jsnapshot)) {
      snapshot_idx = jsonGet<int32_t>(jsnapshot);
    }
    evaluated = makeCV(std::move(id), std::move(evbPath), snapshot_idx);
  } while (false);
  return evaluated;
}

static EvaluablePtr parseProperty(const Json& j) {
  if (JsonTrait::isString(j)) {
    auto expression = JsonTrait::get<String>(j);
    if (!expression.empty() && expression[0] == prefix::property) {
      return makeProp(String(expression));
    }
  }
  return {};
}

static Json reconstructJAS(const Json& j) {
  if (JsonTrait::isArray(j)) {
    ArrayType arr;
    for (auto& ji : jsonGet<ArrayType>(j)) {
      JsonTrait::add(arr, reconstructJAS(ji));
    }
    return JsonTrait::makeJson(std::move(arr));
  } else if (JsonTrait::isObject(j)) {
    ObjectType o;
    for (auto& [key, jval] : jsonGet<ObjectType>(j)) {
      auto _jas = constructJAS(key, reconstructJAS(jval));
      if (JsonTrait::isObject(_jas)) {
        for (auto& [_newKey, _newJval] : jsonGet<ObjectType>(_jas)) {
          JsonTrait::add(o, _newKey, _newJval);
        }
      }
    }
    return JsonTrait::makeJson(std::move(o));
  } else if (JsonTrait::isString(j)) {
    return constructJAS(jsonGet<String>(j));
  } else {
    return j;
  }
}

static Json constructJAS(const StringView& expression) {
  if (!isSpecifierLike(expression) || isSpecifier(expression)) {
    return JsonTrait::makeJson(String{expression});
  }

  auto colonPos = expression.find(':');
  if (colonPos <= expression.size()) {
    auto specifier = expression.substr(0, colonPos);
    auto inputExpr = expression.substr(colonPos + 1);

    auto jexpr = constructJAS(specifier, inputExpr);
    if (!JsonTrait::isNull(jexpr)) {
      return jexpr;
    }
  }
  return JsonTrait::makeJson(String{expression});
}

static Json constructJAS(const StringView& specifier,
                         const StringView& expression) {
  if (isSpecifier(specifier)) {
    return makeJson(specifier, constructJAS(expression));
  }
  return {};
}
static Json constructJAS(const StringView& expression, const Json& jevaluable) {
  if (expression.empty()) {
    return jevaluable;
  } else if (isSpecifier(expression)) {
    return makeJson(expression, jevaluable);
  } else {
    auto colonPos = expression.find(':');
    if (colonPos <= expression.size()) {
      auto specifier = expression.substr(0, colonPos);
      auto inputExpr = expression.substr(colonPos + 1);
      if (isSpecifier(specifier)) {
        return makeJson(specifier, constructJAS(inputExpr, jevaluable));
      }
    }
  }

  return makeJson(expression, jevaluable);
}

static EvaluablePtr parseEVBMap(const Json& j) {
  if (!JsonTrait::isObject(j)) {
    return {};
  }

  auto evbMap = makeEMap();
  String id;
  for (auto& [specifier, jvalue] : JsonTrait::get<ObjectType>(j)) {
    if (specifier == keyword::id) {
      throwIf<SyntaxError>(!JsonTrait::isString(jvalue),
                           "Following id specifier `", specifier,
                           "` must be a string");
      id = specifier;
    } else {
      evbMap->value.emplace(specifier, parser::parseImpl(jvalue));
    }
  }
  return evbMap;
}

static EvaluablePtr parseEVBArray(const Json& j) {
  if (!JsonTrait::isArray(j)) {
    return {};
  }
  auto evbArray = makeEArray();
  for (auto& jevb : JsonTrait::get<ArrayType>(j)) {
    evbArray->value.push_back(parser::parseImpl(jevb));
  }
  return evbArray;
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
      parseEVBArray,
      parseEVBMap,
      parseNoneKeywordVal,  // this function must be call last
      parseInvalidData,
  };
  return _;
}

static EvaluablePtr parseImpl(const Json& j) {
  for (auto& parser : parsers()) {
    if (auto evaled = parser(j)) {
      return evaled;
    }
  }
  return {};
}

EvaluablePtr parse(const Json& j) {
  throwIf<SyntaxError>(JsonTrait::isNull(j),
                       "Not an Evaluable: ", JsonTrait::dump(j));
  auto _jas = reconstructJAS(j);
  //  std::cout << JsonTrait::dump(_jas) << std::endl;
  //  std::cout << "-----------------------------------------" << std::endl;
  return parseImpl(_jas);
}

}  // namespace parser
}  // namespace jas
