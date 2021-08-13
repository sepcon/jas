#include "Parser.h"

#include <algorithm>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string_view>
#include <vector>

#include "CIF.h"
#include "EvalContextIF.h"
#include "EvaluableClasses.h"
#include "Exception.h"
#include "Keywords.h"
#include "Version.h"

namespace jas {

#define JAS_REGEX_VARIABLE_NAME "[a-zA-Z_]+[0-9]*"
#define JAS_REGEX_VARIABLE "\\$" JAS_REGEX_VARIABLE_NAME
#define JAS_REGEX_VARIABLE_FIELD_QUERY \
  R"_regex((\$[a-zA-Z_]+[0-9]*)\[((?:\$[a-zA-Z_]+[0-9]*|[a-zA-Z_0-9\-]+|[\/]*)+)\])_regex"

struct EvaluableInfo {
  String type;
  Json params;
  String id;
};

using std::make_shared;
using std::move;
using OptionalEvbInfo = std::optional<EvaluableInfo>;
using Regex = std::basic_regex<CharType>;
using RegexMatchResult = std::match_results<String::const_iterator>;
using RegexTokenIterator = std::regex_token_iterator<String::const_iterator>;
using StringView = std::basic_string_view<CharType>;

template <typename T>
T jsonGet(const Json& j);
template <typename T>
T jsonGet(const Json& j, const String& key);
template <typename T>
T jsonGet(const Json& j, const String& key);
static Json jsonGet(const Json& j, const String& key);
template <typename T>
T jsonGet(const Json& j, const String& key);
static Json jsonGet(const Json& j, const String& key);
static Json jsonGet(const Json& j, const String& key);
static Json makeJson(const String& key, Json value);
static std::vector<String> split(String str, const String& delim);
struct ParserImpl {
  using ParsingRuleCallback = EvaluablePtr (ParserImpl::*)(const Json&);
  using KeywordParserMap = std::map<String, ParsingRuleCallback>;
  ParserImpl(ContextPtr ctxt) : context_{std::move(ctxt)} {}

  ContextPtr context_;
  std::vector<ParsingRuleCallback> parseCallbacks_ = {
      &ParserImpl::parseNoEval,              //
      &ParserImpl::parseOperations,          //
      &ParserImpl::parseVariableFieldQuery,  //
      &ParserImpl::parseVariable,            //
      &ParserImpl::parseEVBArray,            //
      &ParserImpl::parseEVBMap,              //
      &ParserImpl::parseNoneKeywordVal,      //
  };

  const static std::set<String>& evaluableSpecifiers() {
    static std::set<String> specifiers = {
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
        keyword::filter_if,
        keyword::transform,
    };
    return specifiers;
  }

  static bool isVariableLike(const String& str) {
    return !str.empty() && str[0] == prefix::variable;
  }
  static bool isSpecifierLike(const String& str) {
    return !str.empty() && str[0] == prefix::specifier;
  }

  static bool isEvaluableSpecifier(const String& str) {
    return evaluableSpecifiers().find(str) != evaluableSpecifiers().end();
  }

  static bool isNonOpSpecifier(const String& kw) {
    static const std::set<String> specifiers = {
        keyword::cond,
        keyword::op,
        keyword::list,
        keyword::noeval,
    };
    return specifiers.find(kw) != specifiers.end();
  }

  bool isSpecifier(const String& str) const {
    return !str.empty() &&
           (isEvaluableSpecifier(str) || isNonOpSpecifier(str) ||
            functionSupported(str.substr(1)));
  }

  bool functionSupported(const String& funcName) const {
    return cif::supported(funcName) || context_->functionSupported(funcName);
  }

  void checkSupportedSpecifier(const String& str) {
    if (!isVariableLike(str)) {
      throwIf<SyntaxError>(
          isSpecifierLike(str) && !isSpecifier(str),
          JASSTR("Following character `"), prefix::specifier,
          JASSTR("` must be a specifier or supported function: ??"), str,
          JASSTR("??"));
    }
  }

  static std::optional<EvaluableInfo> extractOperationInfo(const Json& j) {
    if (JsonTrait::isObject(j)) {
      auto oj = jsonGet<JsonObject>(j);
      for (auto& [key, val] : oj) {
        if (!key.empty() && key[0] == prefix::specifier) {
          return EvaluableInfo{key, val,
                               jsonGet<String>(j, String{keyword::id})};
        }
      }
    }
    return {};
  }

  static StackVariablesPtr extractStackVariables(ParserImpl* parser,
                                                 const Json& j) {
    StackVariables stackVariables;
    for (auto& [key, val] : jsonGet<JsonObject>(j)) {
      if (key.size() > 1 && key[0] == prefix::variable) {
        stackVariables.emplace(key.substr(1),
                               VariableInfo{parser->parseImpl(val)});
      }
    }
    if (!stackVariables.empty()) {
      return make_shared<StackVariables>(move(stackVariables));
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
    static EvaluablePtr parse(ParserImpl* parser, const Json& expression,
                              const OptionalEvbInfo& evbInfo) {
      if (evbInfo) {
        auto& [sop, jevaluation, id] = evbInfo.value();
        if (auto op = _Base::mapToEvbType(sop);
            op != _operation_type::invalid) {
          Evaluables params;
          if (JsonTrait::isArray(jevaluation)) {
            for (auto& jparam : jsonGet<JsonArray>(jevaluation)) {
              params.emplace_back(parser->parseImpl(jparam));
            }
          } else {
            params.emplace_back(parser->parseImpl(jevaluation));
          }
          return makeOp(std::move(id), op, std::move(params),
                        extractStackVariables(parser, expression));
        }
      }
      return {};
    }
  };

  struct ArithmeticalOpParser
      : public OperatorParserBase<ArithmeticalOpParser,
                                  ArithmeticOperatorType> {
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
          {keyword::count_if, ListOperationType::count_if},
          {keyword::filter_if, ListOperationType::filter_if},
          {keyword::transform, ListOperationType::transform},
      };
      return _;
    }

    static EvaluablePtr parse(ParserImpl* parser, const Json& expression,
                              const OptionalEvbInfo& evbInfo) {
      EvaluablePtr output;
      do {
        if (!evbInfo) {
          break;
        }
        auto& [sop, jevaluable, id] = evbInfo.value();
        auto op = mapToEvbType(sop);
        if (op == ListOperationType::invalid) {
          break;
        }

        EvaluablePtr evbCond;
        EvaluablePtr list;

        auto listFromCurrentContextData = [parser] {
          return parser->parseImpl(JsonObject{{JASSTR("@field"), JASSTR("")}});
        };

        if (JsonTrait::isObject(jevaluable)) {
          auto jcond = jsonGet(jevaluable, keyword::cond);
          if (JsonTrait::isNull(jcond)) {
            jcond = jsonGet(jevaluable, keyword::op);
          }
          if (JsonTrait::isNull(jcond)) {
            jcond = jevaluable;
          }
          if (!JsonTrait::isNull(jcond)) {
            evbCond = parser->parseImpl(jcond);
          }

          throwIf<SyntaxError>(
              !evbCond, JASSTR("Follow operation `"), sop,
              JASSTR("` must be an evaluable condition with specifier `"),
              keyword::cond, JASSTR("` or `"), keyword::op, "`");

          EvaluablePtr list;
          if (auto jlistExpr = jsonGet(jevaluable, keyword::list);
              JsonTrait::isNull(jlistExpr)) {
            list = listFromCurrentContextData();
          } else {
            list = parser->parseImpl(jlistExpr);
          }

          auto stackVariables = extractStackVariables(parser, expression);
          output = makeOp(id, op, std::move(evbCond), std::move(list),
                          stackVariables);
          break;
        }

        if (JsonTrait::isBool(jevaluable)) {
          output =
              makeOp(id, op, makeDV(jevaluable), listFromCurrentContextData(),
                     extractStackVariables(parser, expression));
        }

      } while (false);
      return output;
    }
  };

  EvaluablePtr parseNoneKeywordVal(const Json& expr) {
    if (JsonTrait::isString(expr)) {
      do {
        auto str = jsonGet<String>(expr);

        auto rend = std::rend(str);
        auto rbegin = std::rbegin(str);
        if (rbegin == rend) {
          break;
        }

        checkSupportedSpecifier(str);

        // Formatting String to number type:
        // "100(%d)", "100(%f), "100(%s)"
        auto itCloseBracket = std::rbegin(str);
        if (*itCloseBracket != JASSTR(')')) {
          break;
        }

        auto itOpenBracket = std::next(itCloseBracket, 3);
        if ((itOpenBracket == rend) || (*itOpenBracket != JASSTR('(')) ||
            (*std::next(itOpenBracket, -1) != JASSTR('%'))) {
          break;
        }
        auto posBeforOpenBracket = str.size() - (itOpenBracket - rbegin) - 1;

        auto typespecifier = *std::next(itCloseBracket, 1);
        auto realVal = String{str.data(), posBeforOpenBracket};
        try {
          switch (typespecifier) {
            case JASSTR('d'):
            case JASSTR('i'):
              return makeDV(std::stoi(realVal));
            case JASSTR('f'):
              return makeDV(std::stod(realVal));
            case JASSTR('l'):
              return makeDV(static_cast<int64_t>(std::stoll(realVal)));
            case JASSTR('s'):
              return makeDV(std::move(realVal));
            default:
              throw_<SyntaxError>("Unrecognized type specifier `",
                                  typespecifier, "` in [", str, "]");
          }
        } catch (const std::exception& e) {
          throw_<SyntaxError>("Cannot convert ", realVal,
                              " to corresponding type [", typespecifier,
                              "]: ", e.what());
        }
      } while (false);
    }
    return makeDV(expr);
  }

  struct FunctionParser {
    static EvaluablePtr parse(ParserImpl* parser, const Json& expr,
                              const OptionalEvbInfo& evbInfo) {
      if (evbInfo) {
        auto& [sop, jevaluable, id] = evbInfo.value();
        if (isSpecifierLike(sop)) {
          auto funcName = sop.substr(1);
          if (parser->functionSupported(funcName)) {
            return makeFnc(std::move(id), move(funcName),
                           parser->parseImpl(jevaluable),
                           extractStackVariables(parser, expr));
          }
        }
      } else if (JsonTrait::isString(expr)) {
        auto str = JsonTrait::get<String>(expr);
        if (!str.empty()) {
          auto funcName = str.substr(1);
          if (parser->functionSupported(funcName)) {
            return makeFnc({}, move(funcName), {});
          }
        }
      }
      return {};
    }
  };

  EvaluablePtr parseNoEval(const Json& expr) {
    if (JsonTrait::isObject(expr) && JsonTrait::hasKey(expr, keyword::noeval)) {
      throwIf<SyntaxError>(JsonTrait::size(expr) > 1,
                           "Json object must contain only ", "keyword `",
                           keyword::noeval, "` and not evaluated expression");
      return makeDV(jsonGet(expr, keyword::noeval));
    }
    return {};
  }

  template <class _OperationParser>
  EvaluablePtr _parseOperation(const Json& expr,
                               const OptionalEvbInfo& evbInfo) {
    return _OperationParser::parse(this, expr, evbInfo);
  }

  EvaluablePtr parseOperations(const Json& expr) {
    using OperationParserCallback = EvaluablePtr (ParserImpl::*)(
        const Json& expr, const OptionalEvbInfo& evbInfo);
    auto evbInfo = extractOperationInfo(expr);
    static std::initializer_list<OperationParserCallback> operationParsers = {
        &ParserImpl::_parseOperation<ArithmeticalOpParser>,
        &ParserImpl::_parseOperation<LogicalOpParser>,
        &ParserImpl::_parseOperation<ComparisonOpParser>,
        &ParserImpl::_parseOperation<ListOpParser>,
        &ParserImpl::_parseOperation<FunctionParser>,
    };
    for (auto parseFunc : operationParsers) {
      if (auto evb = (this->*parseFunc)(expr, evbInfo)) {
        return evb;
      }
    }
    return {};
  }

  EvaluablePtr parseVariable(const Json& j) {
    if (JsonTrait::isString(j)) {
      auto expression = JsonTrait::get<String>(j);
      if (!expression.empty() && (expression[0] == prefix::variable)) {
        return makeProp(expression.substr(1));
      }
    }
    return {};
  }

  EvaluablePtr parseVariableFieldQuery(const Json& j) {
    using namespace std;
    static const Regex rgxVariableFieldQuery{JASSTR(
        R"_regex(\$([a-zA-Z_]+[0-9]*)\[((?:\$[a-zA-Z_]+[0-9]*|[@:a-zA-Z_0-9\-]+|[\/]*)+)\])_regex")};
    static const Regex rgxField{JASSTR(R"_regex([^\/]+)_regex")};
    EvaluablePtr evb;
    do {
      if (!JsonTrait::isString(j)) {
        break;
      }
      auto expression = JsonTrait::get<String>(j);
      if (expression.empty() || (expression[0] != prefix::variable)) {
        break;
      }
      RegexMatchResult matches;
      if (!regex_match(expression, matches, rgxVariableFieldQuery) ||
          (matches.size() != 3)) {
        break;
      }

      auto varName = String{matches[1].first, matches[1].second};
      auto& fieldPathMatch = matches[2];
      auto fieldIt = RegexTokenIterator(fieldPathMatch.first,
                                        fieldPathMatch.second, rgxField);
      RegexTokenIterator fieldEnd;

      vector<EvaluablePtr> paths;
      while (fieldIt != fieldEnd) {
        if (fieldIt->matched) {
          auto subExpr = fieldIt->str();
          Json jsubExpr;
          if (subExpr.find(JASSTR(':')) != String::npos) {
            jsubExpr = this->constructJAS(subExpr);
          } else {
            jsubExpr = subExpr;
          }

          paths.push_back(parseImpl(jsubExpr));
        }
        ++fieldIt;
      }
      evb = makeVariableFieldQuery(move(varName), move(paths));
    } while (false);
    return evb;
  }

  Json reconstructJAS(const Json& j) {
    if (JsonTrait::isArray(j)) {
      JsonArray arr;
      for (auto& val : jsonGet<JsonArray>(j)) {
        JsonTrait::add(arr, reconstructJAS(val));
      }
      return JsonTrait::makeJson(std::move(arr));
    } else if (JsonTrait::isObject(j)) {
      JsonObject o;
      for (auto& [key, jval] : jsonGet<JsonObject>(j)) {
        auto _jas = constructJAS(key, reconstructJAS(jval));
        if (JsonTrait::isObject(_jas)) {
          for (auto& [_newKey, _newJval] : jsonGet<JsonObject>(_jas)) {
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

  Json constructJAS(const String& expression) {
    if (expression.empty()) {
      return {};
    }

    if (isSpecifier(expression)) {
      return JsonTrait::makeJson(expression);
    }

    auto increaseTillNotSpace = [](const String& s, size_t& currentPos,
                                   int step = 1) {
      while (::isspace(s[currentPos])) {
        if (currentPos == 0 && step < 0) {
          currentPos = String::npos;
          break;
        }
        currentPos += step;
      }
    };
    auto nextSpecifierPos = expression.find(':');
    if (nextSpecifierPos < expression.size()) {
      auto thisSpecifierEndPos = nextSpecifierPos - 1;

      increaseTillNotSpace(expression, thisSpecifierEndPos, -1);
      ++nextSpecifierPos;
      increaseTillNotSpace(expression, nextSpecifierPos);
      auto specifier = expression.substr(0, thisSpecifierEndPos + 1);
      auto inputExpr = expression.substr(nextSpecifierPos);
      auto jexpr = constructJAS(specifier, inputExpr);
      if (!JsonTrait::isNull(jexpr)) {
        return jexpr;
      }
    }
    size_t b = 0;
    size_t e = expression.size() - 1;
    increaseTillNotSpace(expression, b);
    increaseTillNotSpace(expression, e, -1);
    if ((e - b) == expression.size()) {
      return JsonTrait::makeJson(expression);
    } else if (b <= e) {
      return JsonTrait::makeJson(expression.substr(b, e + 1));
    } else {
      return {};
    }
  }

  Json constructJAS(const String& specifier, const String& expression) {
    if (isSpecifier(specifier)) {
      return makeJson(specifier, constructJAS(expression));
    }
    return {};
  }

  template <class Iterator>
  Json makeJAS(Iterator specifierBeg, Iterator specifierEnd,
               const Json& jevaluable) {
    if (specifierBeg != specifierEnd) {
      auto curIt = specifierBeg;
      return makeJson(*curIt,
                      makeJAS(++specifierBeg, specifierEnd, jevaluable));
    } else {
      return jevaluable;
    }
  }

  Json constructJAS(const String& expression, const Json& jevaluable) {
    if (expression == keyword::noeval) {
      return JsonTrait::makeJson(JsonObject{{expression, jevaluable}});
    } else if (isSpecifier(expression)) {
      return makeJson(expression, jevaluable);
    } else {
      auto tokens = split(expression, JASSTR(":"));
      if (!tokens.empty()) {
        auto isPipeline = true;
        for (auto it = tokens.begin() + 1; it != tokens.end(); ++it) {
          checkSupportedSpecifier(*it);
          if (!isSpecifierLike(*it) && !isVariableLike(*it)) {
            isPipeline = false;
            break;
          }
        }

        if (isPipeline) {
          checkSupportedSpecifier(tokens.front());
          return makeJAS(tokens.begin(), tokens.end(), jevaluable);
        }
      }
    }

    checkSupportedSpecifier(expression);
    return makeJson(expression, jevaluable);
  }

  EvaluablePtr parseEVBMap(const Json& j) {
    if (!JsonTrait::isObject(j)) {
      return {};
    }

    auto evbMap = makeEMap();
    String id;
    StackVariables stackVariables;
    for (auto& [key, jvalue] : JsonTrait::get<JsonObject>(j)) {
      if (key == keyword::id) {
        throwIf<SyntaxError>(!JsonTrait::isString(jvalue),
                             JASSTR("Following id specifier `"), key,
                             JASSTR("` must be a string"));
        evbMap->id = jsonGet<String>(jvalue);
      } else if (!key.empty() && key[0] == prefix::variable) {
        stackVariables.emplace(key.substr(1), parseImpl(jvalue));
      } else {
        evbMap->value.emplace(key, parseImpl(jvalue));
      }
    }
    if (!stackVariables.empty()) {
      evbMap->stackVariables =
          make_shared<StackVariables>(std::move(stackVariables));
    }
    return evbMap;
  }

  EvaluablePtr parseEVBArray(const Json& j) {
    if (!JsonTrait::isArray(j)) {
      return {};
    }
    auto evbArray = makeEArray();
    for (auto& jevb : JsonTrait::get<JsonArray>(j)) {
      evbArray->value.push_back(parseImpl(jevb));
    }
    return evbArray;
  }

  EvaluablePtr parseImpl(const Json& j) {
    for (auto parse : parseCallbacks_) {
      if (auto evaled = (this->*parse)(j)) {
        return evaled;
      }
    }
    return {};
  }

  void checkVersionCompatibility(const Json& jas) {
    auto exprVer = jsonGet<String>(jas, JASSTR("$jas.version"));
    if (!exprVer.empty()) {
      int cmp =
          cif::invoke(JASSTR("cmp_ver"), JsonArray{exprVer, jas::version});
      throwIf<SyntaxError>(
          cmp > 0,
          "Given JAS syntax has higher version than current JAS parser, "
          "current[",
          jas::version, "], given[", exprVer, "]");
    }
  }

  EvaluablePtr parse(const Json& jas, parser::Strategy strategy) {
    throwIf<SyntaxError>(JsonTrait::isNull(jas), JASSTR("Not an Evaluable: "),
                         JsonTrait::dump(jas));
    checkVersionCompatibility(jas);
    if (strategy == parser::Strategy::AllowShorthand) {
      auto _jas = reconstructJAS(jas);
      return parseImpl(_jas);
    } else {
      return parseImpl(jas);
    }
  }
};

template <typename T>
T jsonGet(const Json& j) {
  return JsonTrait::template get<T>(j);
}
template <typename T>
T jsonGet(const Json& j, const String& key) {
  return jsonGet<T>(jsonGet(j, key));
}

static Json jsonGet(const Json& j, const String& key) {
  return JsonTrait::get(j, key);
}

static Json makeJson(const String& key, Json value) {
  JsonObject o;
  JsonTrait::add(o, String{key}, std::move(value));
  return JsonTrait::makeJson(std::move(o));
}

static std::vector<String> split(String str, const String& delim) {
  std::vector<String> tokens;
  do {
    auto colonPos = str.find(delim);
    if (colonPos < str.size()) {
      tokens.push_back(str.substr(0, colonPos));
      str = str.substr(colonPos + 1);
    } else {
      if (!str.empty()) {
        tokens.push_back(str);
      }
      break;
    }
  } while (true);
  return tokens;
}

namespace parser {
EvaluablePtr parse(EvalContextPtr ctxt, const Json& jas, Strategy strategy) {
  return ParserImpl{std::move(ctxt)}.parse(jas, strategy);
}

Json reconstructJAS(EvalContextPtr ctxt, const Json& jas) {
  return ParserImpl{ctxt}.reconstructJAS(jas);
}

const std::set<String>& evaluableSpecifiers() {
  return ParserImpl::evaluableSpecifiers();
}

}  // namespace parser

// namespace parser
}  // namespace jas
