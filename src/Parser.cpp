#include "jas/Parser.h"

#include <algorithm>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string_view>
#include <vector>

#include "Builtin.ListOps.h"
#include "jas/CIF.h"
#include "jas/EvalContextIF.h"
#include "jas/EvaluableClasses.h"
#include "jas/Exception.h"
#include "jas/Keywords.h"
#include "jas/Version.h"

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
static Json makeJson(const StringView& key, Json value);
static std::vector<StringView> split(StringView str, const String& delim);
struct ParserImpl {
  using ParsingRuleCallback = EvaluablePtr (ParserImpl::*)(const Json&);
  using KeywordParserMap = std::map<String, ParsingRuleCallback>;
  ParserImpl(ContextPtr ctxt) : context_{std::move(ctxt)} {}

  ContextPtr context_;
  std::vector<ParsingRuleCallback> parseCallbacks_ = {
      &ParserImpl::parseNoEffectOperations,  //
      &ParserImpl::parseOperations,          //
      &ParserImpl::parseVariableFieldQuery,  //
      &ParserImpl::parseVariable,            //
      &ParserImpl::parseEVBArray,            //
      &ParserImpl::parseEVBMap,              //
      &ParserImpl::parseNoneKeywordVal,      //
  };

  const static std::set<StringView>& evaluableSpecifiers() {
    using namespace keyword;
#define __JAS_KEWORD_DECLARATION 1
    static std::set<StringView> specifiers = {
#include "jas/Keywords.dat"
    };
    return specifiers;
  }

  static bool isVariableLike(const StringView& str) {
    return !str.empty() && str[0] == prefix::variable;
  }
  static bool isSpecifierLike(const StringView& str) {
    return !str.empty() && str[0] == prefix::specifier;
  }

  static bool isEvaluableSpecifier(const StringView& str) {
    return evaluableSpecifiers().find(str) != std::end(evaluableSpecifiers());
  }

  static bool isNonOpSpecifier(const StringView& kw) {
    using namespace keyword;
    static const std::set<StringView> specifiers = {
#define __JAS_KEWORD_DECLARATION 2
#include "jas/Keywords.dat"
    };
    return specifiers.find(kw) != std::end(specifiers);
  }

  bool isSpecifier(const StringView& str) const {
    return !str.empty() &&
           (isEvaluableSpecifier(str) || isNonOpSpecifier(str) ||
            functionSupported({}, str.substr(1)));
  }

  bool isJasFunction(const StringView& module,
                     const StringView& funcName) const {
    return funcName == keyword::return_ + 1;
  }

  bool isBuiltinFunction(const StringView& module,
                         const StringView& funcName) const {
    using SupportCheckCallback = bool (*)(const StringView& func);
    static std::map<String, SupportCheckCallback, std::less<>> _funcCheckMap = {
        {builtin::list::moduleName(), builtin::list::supported}};
    auto it = _funcCheckMap.find(module);
    return it != std::end(_funcCheckMap) && it->second(funcName);
  }

  bool functionSupported(const StringView& module,
                         const StringView& funcName) const {
    return isJasFunction(module, funcName) ||
           cif::supported(module, funcName) ||
           context_->functionSupported(funcName) ||
           isBuiltinFunction(module, funcName);
  }

  void throwIfFunctionNotSupported(const StringView& moduleName,
                                   const StringView& funcName) {
    throwIf<SyntaxError>(!functionSupported(moduleName, funcName),
                         "Unknown reference to function: `@", moduleName, ".",
                         funcName, "` check registering module `", moduleName,
                         "` to CIF or defined your own function");
  }
  void checkSupportedSpecifier(const StringView& str) {
    if (!isVariableLike(str)) {
      throwIf<SyntaxError>(
          isSpecifierLike(str) && !isSpecifier(str),
          JASSTR("Following character `"), prefix::specifier,
          JASSTR("` must be a specifier or supported function: ??"), str,
          JASSTR("??"));
    }
  }

  static std::optional<EvaluableInfo> extractOperationInfo(const Json& j) {
    std::optional<EvaluableInfo> evbi;
    if (JsonTrait::isObject(j)) {
      JsonTrait::iterateObject(j, [&](const String& key, const Json& val) {
        if (!key.empty() && key[0] == prefix::specifier) {
          evbi = EvaluableInfo{key, val,
                               JsonTrait::get<String>(j, String{keyword::id})};
          return false;  // break;
        } else {
          return true;
        }
      });
    }
    return evbi;
  }

  static std::optional<VariableInfo> extractStackVariables(
      ParserImpl* parser, const String& key, const Json& evbExpr) {
    if (key.size() > 1 && key[0] == prefix::variable) {
      String::size_type namePos = 1;
      VariableInfo::Type type = VariableInfo::Declaration;
      if (key[namePos] == JASSTR('+')) {
        ++namePos;
        type = VariableInfo::Update;
      }
      return VariableInfo{key.substr(namePos), parser->parseImpl(evbExpr),
                          type};
    }
    return {};
  }

  static StackVariablesPtr extractStackVariables(ParserImpl* parser,
                                                 const Json& j) {
    StackVariables stackVariables;
    JsonTrait::iterateObject(j, [&](auto&& key, auto&& val) {
      if (key.size() > 1 && key[0] == prefix::variable) {
        if (auto vi = extractStackVariables(parser, key, val); vi.has_value()) {
          stackVariables.insert(move(*vi));
        }
      }
      return true;  // iterate all
    });

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
            JsonTrait::iterateArray(jevaluation, [&](auto&& jparam) {
              params.emplace_back(parser->parseImpl(jparam));
              return true;
            });
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
      : public OperatorParserBase<ArithmeticalOpParser, aot> {
    static const auto& s2op_map() {
      static std::map<String, aot> _ = {
          {keyword::plus, aot::plus},
          {keyword::minus, aot::minus},
          {keyword::multiplies, aot::multiplies},
          {keyword::divides, aot::divides},
          {keyword::modulus, aot::modulus},
          {keyword::bit_and, aot::bit_and},
          {keyword::bit_or, aot::bit_or},
          {keyword::bit_not, aot::bit_not},
          {keyword::bit_xor, aot::bit_xor},
          {keyword::negate, aot::negate},
      };
      return _;
    }
  };

  struct ArthmSelfAssignOperatorParser
      : public OperatorParserBase<ArthmSelfAssignOperatorParser, asot> {
    static const auto& s2op_map() {
      static std::map<String, asot> _ = {
          {keyword::s_plus, asot::s_plus},
          {keyword::s_minus, asot::s_minus},
          {keyword::s_multiplies, asot::s_multiplies},
          {keyword::s_divides, asot::s_divides},
          {keyword::s_modulus, asot::s_modulus},
          {keyword::s_bit_and, asot::s_bit_and},
          {keyword::s_bit_or, asot::s_bit_or},
          {keyword::s_bit_xor, asot::s_bit_xor},
      };
      return _;
    }
  };
  struct LogicalOpParser : public OperatorParserBase<LogicalOpParser, lot> {
    static const auto& s2op_map() {
      static std::map<String, lot> _ = {
          {keyword::logical_and, lot::logical_and},
          {keyword::logical_or, lot::logical_or},
          {keyword::logical_not, lot::logical_not},
      };
      return _;
    }
  };
  struct ComparisonOpParser
      : public OperatorParserBase<ComparisonOpParser, cot> {
    static const auto& s2op_map() {
      static std::map<String, cot> _ = {
          {keyword::eq, cot::eq}, {keyword::neq, cot::neq},
          {keyword::lt, cot::lt}, {keyword::gt, cot::gt},
          {keyword::le, cot::le}, {keyword::ge, cot::ge},
      };
      return _;
    }
  };

  struct ListOpParser : public OperationParserBase<ListOpParser, lsot> {
    static const auto& s2op_map() {
      static std::map<String, lsot> _ = {
          {keyword::any_of, lsot::any_of},
          {keyword::all_of, lsot::all_of},
          {keyword::none_of, lsot::none_of},
          {keyword::count_if, lsot::count_if},
          {keyword::filter_if, lsot::filter_if},
          {keyword::transform, lsot::transform},
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
        if (op == lsot::invalid) {
          break;
        }

        EvaluablePtr evbCond;
        EvaluablePtr list;

        auto listFromCurrentContextData = [] {
          return makeFnc({}, JASSTR("field"));
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
        // "100(%d)", "100(%f), "100(%s), "true(%b)"
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
            case JASSTR('b'):
              if (realVal == JASSTR("true")) {
                return makeDV(true);
              } else if (realVal == JASSTR("false")) {
                return makeDV(false);
              } else {
                throw_<SyntaxError>("specifier %b cannot apply to ", realVal);
              }
              break;
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
      auto splitModule = [](const StringView& str) {
        StringView funcName{str.data() + 1, str.size() - 1};
        StringView moduleName;
        auto dotPos = funcName.find_last_of(JASSTR('.'));
        if (dotPos != StringView::npos) {
          moduleName = funcName.substr(0, dotPos);
          funcName = funcName.substr(dotPos + 1);
          throwIf<SyntaxError>(moduleName.empty(),
                               "Module name must not empty `", str, "`");
          throwIf<SyntaxError>(
              funcName.empty(),
              "FunctionInvocation name after module-separator(.) must not be empty `",
              str, "`");
          return std::make_pair(funcName, moduleName);
        } else {
          return std::make_pair(funcName, StringView{JASSTR("")});
        }
      };

      if (evbInfo) {
        auto& [sop, jevaluable, id] = evbInfo.value();
        if (isSpecifierLike(sop)) {
          auto [funcName, moduleName] = splitModule(sop);
          parser->throwIfFunctionNotSupported(moduleName, funcName);
          return makeFnc(id, String(funcName), parser->parseImpl(jevaluable),
                         String{moduleName},
                         extractStackVariables(parser, expr));
        }
      } else if (JsonTrait::isString(expr)) {
        auto str = JsonTrait::get<String>(expr);
        if (!str.empty() && parser->isSpecifier(str)) {
          auto [funcName, moduleName] = splitModule(str);
          parser->throwIfFunctionNotSupported(moduleName, funcName);
          return makeFnc({}, String(funcName), {}, String{moduleName});
        }
      }
      return {};
    }
  };

  EvaluablePtr parseNoEffectOperations(const Json& expr) {
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
        &ParserImpl::_parseOperation<ArthmSelfAssignOperatorParser>,
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
      auto arr = JsonTrait::array();
      JsonTrait::iterateArray(j, [&](auto&& val) {
        JsonTrait::add(arr, reconstructJAS(val));
        return true;
      });
      return JsonTrait::makeJson(std::move(arr));
    } else if (JsonTrait::isObject(j)) {
      auto reconstructedObject = JsonTrait::object();
      JsonTrait::iterateObject(j, [&](auto&& key, auto&& val) {
        auto _jas = constructJAS(key, reconstructJAS(val));
        if (JsonTrait::isObject(_jas)) {
          JsonTrait::iterateObject(_jas, [&](auto&& _newKey, auto&& _newJval) {
            JsonTrait::add(reconstructedObject, _newKey, _newJval);
            return true;
          });
        }
        return true;
      });
      return JsonTrait::makeJson(std::move(reconstructedObject));
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
      return JsonTrait::object({{expression, jevaluable}});
    } else if (isSpecifier(expression)) {
      return makeJson(expression, jevaluable);
    } else {
      auto tokens = split(expression, JASSTR(":"));
      if (!tokens.empty()) {
        auto isPipeline = true;
        for (auto it = tokens.begin() + 1; it != tokens.end(); ++it) {
          if (!isSpecifierLike(*it) && !isVariableLike(*it)) {
            isPipeline = false;
            break;
          }
        }

        if (isPipeline) {
          return makeJAS(tokens.begin(), tokens.end(), jevaluable);
        }
      }
    }

    return makeJson(expression, jevaluable);
  }

  EvaluablePtr parseEVBMap(const Json& j) {
    EvaluablePtr evb;

    do {
      if (!JsonTrait::isObject(j)) {
        break;
      }
      auto evbMap = makeEMap();
      String id;
      StackVariables stackVariables;
      JsonTrait::iterateObject(j, [&](auto&& key, auto&& jvalue) {
        if (key == keyword::id) {
          throwIf<SyntaxError>(!JsonTrait::isString(jvalue),
                               JASSTR("Following id specifier `"), key,
                               JASSTR("` must be a string"));
          evbMap->id = jsonGet<String>(jvalue);
        } else if (auto vi = extractStackVariables(this, key, jvalue);
                   vi.has_value()) {
          stackVariables.insert(move(*vi));
        } else {
          evbMap->value.emplace(key, parseImpl(jvalue));
        }
        return true;
      });

      if (!stackVariables.empty()) {
        evbMap->stackVariables =
            make_shared<StackVariables>(std::move(stackVariables));
      } else if (evbMap->id.empty()) {
        if (std::all_of(
                std::begin(evbMap->value), std::end(evbMap->value),
                [](auto&& pair) { return isType<DirectVal>(pair.second); })) {
          evb = makeDV(j);
          break;
        }
      }
      evb = evbMap;
    } while (false);
    return evb;
  }

  EvaluablePtr parseEVBArray(const Json& j) {
    if (!JsonTrait::isArray(j)) {
      return {};
    }

    EvaluableArray::value_type arr;
    JsonTrait::iterateArray(j, [&](auto&& jevb) {
      arr.push_back(parseImpl(jevb));
      return true;
    });
    if (std::all_of(std::begin(arr), std::end(arr),
                    [](auto&& e) { return isType<DirectVal>(e); })) {
      return makeDV(j);
    } else {
      return makeEArray(move(arr));
    }
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
      int cmp = cif::invoke(JASSTR(""), JASSTR("cmp_ver"),
                            JsonTrait::array({exprVer, jas::version}));
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
};  // namespace jas

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

static Json makeJson(const StringView& key, Json value) {
  return JsonTrait::object({{String{key}, std::move(value)}});
}

static std::vector<StringView> split(StringView str, const String& delim) {
  std::vector<StringView> tokens;
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

const std::set<StringView>& evaluableSpecifiers() {
  return ParserImpl::evaluableSpecifiers();
}

}  // namespace parser

// namespace parser
}  // namespace jas
