#include "jas/Translator.h"

#include <algorithm>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string_view>
#include <vector>

#include "jas/EvalContextIF.h"
#include "jas/EvaluableClasses.h"
#include "jas/Exception.h"
#include "jas/FunctionModule.h"
#include "jas/Keywords.h"
#include "jas/Module.CIF.h"
#include "jas/ModuleManager.h"
#include "jas/Version.h"

namespace jas {

#define JAS_REGEX_VARIABLE_NAME "[a-zA-Z_]+[0-9]*"
#define JAS_REGEX_VARIABLE "\\$" JAS_REGEX_VARIABLE_NAME
#define JAS_REGEX_VARIABLE_FIELD_QUERY \
  R"_regex((\$[a-zA-Z_]+[0-9]*)\[((?:\$[a-zA-Z_]+[0-9]*|[a-zA-Z_0-9\-]+|[\/]*)+)\])_regex"

struct EvaluableInfo {
  String type;
  Var params;
  String id;
};

using std::make_shared;
using std::move;
using OptionalEvbInfo = std::optional<EvaluableInfo>;
using Regex = std::basic_regex<CharType>;
using RegexMatchResult = std::match_results<String::const_iterator>;
using RegexTokenIterator = std::regex_token_iterator<String::const_iterator>;

static std::vector<StringView> split(StringView str, const String& delim);
struct TranslatorImpl {
  using ParsingRuleCallback = EvaluablePtr (TranslatorImpl::*)(const Var&);
  using KeywordTranslatorMap = std::map<String, ParsingRuleCallback>;
  TranslatorImpl(ModuleManager* moduleMgr) : moduleMgr_(moduleMgr) {}

  ModuleManager* moduleMgr_;
  ContextPtr context_;
  std::vector<ParsingRuleCallback> translateCallbacks_ = {
      &TranslatorImpl::translateNoEffectOperations,  //
      &TranslatorImpl::translateOperations,          //
      &TranslatorImpl::translateVariable,            //
      &TranslatorImpl::translateEVBList,             //
      &TranslatorImpl::translateEVBDict,             //
      &TranslatorImpl::translateNoneKeywordVal,      //
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

  bool functionSupported(const StringView& module,
                         const StringView& funcName) const {
    return isJasFunction(module, funcName) ||
           (context_ && context_->functionSupported(funcName)) ||
           moduleMgr_->hasFunction(module, funcName);
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

  static std::optional<EvaluableInfo> extractOperationInfo(const Var& j) {
    std::optional<EvaluableInfo> evbi;
    if (j.isDict()) {
      for (auto& [key, val] : j.asDict()) {
        if (!key.empty() && key[0] == prefix::specifier) {
          evbi = EvaluableInfo{key, val, j.getAt(keyword::id).getString()};
          break;
        }
      }
    }
    return evbi;
  }

  static std::optional<VariableInfo> extractStackVariables(
      TranslatorImpl* translater, const String& key, const Var& evbExpr) {
    if (key.size() > 1 && key[0] == prefix::variable) {
      String::size_type namePos = 1;
      VariableInfo::Type type = VariableInfo::Declaration;
      if (key[namePos] == JASSTR('+')) {
        ++namePos;
        type = VariableInfo::Update;
      }
      return VariableInfo{key.substr(namePos),
                          translater->translateImpl(evbExpr), type};
    }
    return {};
  }

  static StackVariablesPtr extractStackVariables(TranslatorImpl* translater,
                                                 const Var& j) {
    StackVariables stackVariables;
    if (j.isDict()) {
      for (auto& [key, val] : j.asDict()) {
        if (key.size() > 1 && key[0] == prefix::variable) {
          if (auto vi = extractStackVariables(translater, key, val);
              vi.has_value()) {
            stackVariables.insert(move(*vi));
          }
        }
      }
    }

    if (!stackVariables.empty()) {
      return make_shared<StackVariables>(move(stackVariables));
    }
    return {};
  }

  template <class _op_translater, class _operation_type>
  struct OperationTranslatorBase {
    using operation_type = _operation_type;
    static operation_type mapToEvbType(const String& sop) {
      if (auto it = _op_translater::s2op_map().find(sop);
          it != std::end(_op_translater::s2op_map())) {
        return it->second;
      } else {
        return operation_type::invalid;
      }
    }
  };

  template <class _op_translater, class _operation_type>
  struct OperatorTranslatorBase
      : public OperationTranslatorBase<_op_translater, _operation_type> {
    using _Base = OperationTranslatorBase<_op_translater, _operation_type>;
    static EvaluablePtr translate(TranslatorImpl* translater,
                                  const Var& expression,
                                  const OptionalEvbInfo& evbInfo) {
      if (evbInfo) {
        auto& [sop, jevaluation, id] = evbInfo.value();
        if (auto op = _Base::mapToEvbType(sop);
            op != _operation_type::invalid) {
          Evaluables params;
          if (jevaluation.isList()) {
            for (auto& item : jevaluation.asList()) {
              params.emplace_back(translater->translateImpl(item));
            }
          } else {
            params.emplace_back(translater->translateImpl(jevaluation));
          }
          return makeOp(id, op, std::move(params),
                        extractStackVariables(translater, expression));
        }
      }
      return {};
    }
  };

  struct ArithmeticalOpTranslator
      : public OperatorTranslatorBase<ArithmeticalOpTranslator, aot> {
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

  struct ArthmSelfAssignOperatorTranslator
      : public OperatorTranslatorBase<ArthmSelfAssignOperatorTranslator, asot> {
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
  struct LogicalOpTranslator
      : public OperatorTranslatorBase<LogicalOpTranslator, lot> {
    static const auto& s2op_map() {
      static std::map<String, lot> _ = {
          {keyword::logical_and, lot::logical_and},
          {keyword::logical_or, lot::logical_or},
          {keyword::logical_not, lot::logical_not},
      };
      return _;
    }
  };
  struct ComparisonOpTranslator
      : public OperatorTranslatorBase<ComparisonOpTranslator, cot> {
    static const auto& s2op_map() {
      static std::map<String, cot> _ = {
          {keyword::eq, cot::eq}, {keyword::neq, cot::neq},
          {keyword::lt, cot::lt}, {keyword::gt, cot::gt},
          {keyword::le, cot::le}, {keyword::ge, cot::ge},
      };
      return _;
    }
  };

  struct ListOpTranslator
      : public OperationTranslatorBase<ListOpTranslator, lsot> {
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

    static EvaluablePtr translate(TranslatorImpl* translater,
                                  const Var& expression,
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
          return makeSimpleFI<ContextFI>({}, JASSTR("field"));
        };

        if (jevaluable.isDict()) {
          auto jcond = jevaluable.getAt(keyword::cond);
          if (jcond.isNull()) {
            jcond = jevaluable.getAt(keyword::op);
          }
          if (jcond.isNull()) {
            jcond = jevaluable;
          }
          if (!jcond.isNull()) {
            evbCond = translater->translateImpl(jcond);
          }

          throwIf<SyntaxError>(
              !evbCond, JASSTR("Follow operation `"), sop,
              JASSTR("` must be an evaluable condition with specifier `"),
              keyword::cond, JASSTR("` or `"), keyword::op, "`");

          EvaluablePtr list;
          if (auto jlistExpr = jevaluable.getAt(keyword::list);
              jlistExpr.isNull()) {
            list = listFromCurrentContextData();
          } else {
            list = translater->translateImpl(jlistExpr);
          }

          auto stackVariables = extractStackVariables(translater, expression);
          output = makeOp(id, op, std::move(evbCond), std::move(list),
                          stackVariables);
          break;
        }

        if (jevaluable.isBool()) {
          output =
              makeOp(id, op, makeDV(jevaluable), listFromCurrentContextData(),
                     extractStackVariables(translater, expression));
        }

      } while (false);
      return output;
    }
  };

  EvaluablePtr translateNoneKeywordVal(const Var& expr) {
    do {
      if (!expr.isString()) {
        break;
      }
      auto& str = expr.asString();

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
            throw_<SyntaxError>("Unrecognized type specifier `", typespecifier,
                                "` in [", str, "]");
        }
      } catch (const std::exception& e) {
        throw_<SyntaxError>("Cannot convert ", realVal,
                            " to corresponding type [", typespecifier,
                            "]: ", e.what());
      }
    } while (false);
    return makeDV(expr);
  }

  bool isContextFI(const StringView& module, const StringView& funcName) const {
    if (context_ && module.empty()) {
      return context_->functionSupported(funcName);
    }
    return false;
  }

  struct FunctionTranslator {
    static EvaluablePtr translate(TranslatorImpl* translater, const Var& expr,
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
          throwIf<SyntaxError>(funcName.empty(),
                               "FunctionInvocation name after "
                               "module-separator(.) must not be empty `",
                               str, "`");
          return std::make_pair(funcName, moduleName);
        } else {
          return std::make_pair(funcName, StringView{JASSTR("")});
        }
      };

      String id;
      String funcName;
      String moduleName;
      const Var* jStackVarsExpr = nullptr;
      const Var* jParamExpr = nullptr;

      auto _makeFunction = [&]() -> EvaluablePtr {
        auto _translateParam = [&] {
          if (jParamExpr) {
            return translater->translateImpl(*jParamExpr);
          }
          return EvaluablePtr{};
        };
        auto _translateStackVars = [&] {
          if (jStackVarsExpr) {
            return extractStackVariables(translater, *jStackVarsExpr);
          }
          return StackVariablesPtr{};
        };

        if (translater->isContextFI(moduleName, funcName)) {
          return makeSimpleFI<ContextFI>(
              id, String(funcName), _translateParam(), _translateStackVars());
        } else if (translater->isJasFunction(moduleName, funcName)) {
          return makeSimpleFI<EvaluatorFI>(
              id, String(funcName), _translateParam(), _translateStackVars());

        } else if (auto module = translater->moduleMgr_->getModule(moduleName);
                   module) {
          throwIf<SyntaxError>(!module->has(funcName),
                               "There's no functio named `", funcName,
                               "` in module `", moduleName, "`");

          return makeModuleFI(id, String(funcName), _translateParam(),
                              move(module), _translateStackVars());
        }
        throw_<SyntaxError>("Unknown reference to function: `@", moduleName,
                            ".", funcName,
                            "` Please add custom module to JAS for extending "
                            "its functionality");
        return {};
      };

      if (evbInfo) {
        auto& [sop, jevaluable, id] = evbInfo.value();
        if (isSpecifierLike(sop)) {
          std::tie(funcName, moduleName) = splitModule(sop);
          jStackVarsExpr = &expr;
          jParamExpr = &jevaluable;
          return _makeFunction();
        }
      } else if (expr.isString()) {
        auto& str = expr.asString();
        if (!str.empty() && translater->isSpecifierLike(str)) {
          std::tie(funcName, moduleName) = splitModule(str);
          return _makeFunction();
        }
      }
      return {};
    }
  };

  EvaluablePtr translateNoEffectOperations(const Var& expr) {
    if (expr.isDict() && expr.contains(keyword::noeval)) {
      throwIf<SyntaxError>(expr.size() > 1, "Object must contain only ",
                           "keyword `", keyword::noeval,
                           "` and not evaluated expression");
      return makeDV(expr.at(keyword::noeval));
    }
    return {};
  }

  template <class _OperationTranslator>
  EvaluablePtr _translateOperation(const Var& expr,
                                   const OptionalEvbInfo& evbInfo) {
    return _OperationTranslator::translate(this, expr, evbInfo);
  }

  EvaluablePtr translateOperations(const Var& expr) {
    using OperationTranslatorCallback = EvaluablePtr (TranslatorImpl::*)(
        const Var& expr, const OptionalEvbInfo& evbInfo);
    auto evbInfo = extractOperationInfo(expr);
    static std::initializer_list<OperationTranslatorCallback>
        operationTranslators = {
            &TranslatorImpl::_translateOperation<ArithmeticalOpTranslator>,
            &TranslatorImpl::_translateOperation<LogicalOpTranslator>,
            &TranslatorImpl::_translateOperation<ComparisonOpTranslator>,
            &TranslatorImpl::_translateOperation<ListOpTranslator>,
            &TranslatorImpl::_translateOperation<
                ArthmSelfAssignOperatorTranslator>,
            &TranslatorImpl::_translateOperation<FunctionTranslator>,
        };
    for (auto translateFunc : operationTranslators) {
      if (auto evb = (this->*translateFunc)(expr, evbInfo)) {
        return evb;
      }
    }
    return {};
  }

  EvaluablePtr translateVariable(const Var& j) {
    EvaluablePtr evb;
    do {
      if (!j.isString()) {
        break;
      }
      decltype(auto) expression = j.asString();
      if (!isVariableLike(expression)) {
        break;
      }
      if (evb = _translateVariableFieldQuery(expression); evb) {
        break;
      }

      return makeProp(expression.substr(1));
    } while (false);
    return evb;
  }

  //  std::shared_ptr<ModuleFI> translateVariableMethodCall(const String&
  //  expression) {
  //    assert(!expression.empty());
  //    BasicObjectPathView<JASSTR('.')> query = expression;
  //    auto it = std::begin(query);
  //    auto queryEnd = std::end(query);
  //    auto var = *it;
  //    StringView method;
  //    if (++it != queryEnd) {
  //      method = *it;
  //    }
  //    if (++it == queryEnd) {
  //      makeModuleFI({}, {}, )
  //    }
  //  }

  EvaluablePtr _translateVariableFieldQuery(const String& expression) {
    using namespace std;
    static const Regex rgxVariableFieldQuery{JASSTR(
        R"_regex(\$([a-zA-Z_]+[0-9]*)\[((?:\$[a-zA-Z_]+[0-9]*|[@:a-zA-Z_0-9\-]+|[\/]*)+)\])_regex")};
    static const Regex rgxField{JASSTR(R"_regex([^\/]+)_regex")};

    assert(isVariableLike(expression));
    EvaluablePtr evb;
    do {
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

      vector<EvaluablePtr> path;
      while (fieldIt != fieldEnd) {
        if (fieldIt->matched) {
          auto subExpr = fieldIt->str();
          Var jsubExpr;
          if (subExpr.find(JASSTR(':')) != String::npos) {
            jsubExpr = this->constructJAS(subExpr);
          } else {
            jsubExpr = subExpr;
          }

          path.push_back(translateImpl(jsubExpr));
        }
        ++fieldIt;
      }
//      evb = makeModuleFI({}, JASSTR("dict"), )
      evb = makeVariableFieldQuery(move(varName), move(path));
    } while (false);
    return evb;
  }

  Var reconstructJAS(const Var& j) {
    if (j.isList()) {
      auto arr = Var::List{};
      for (auto& item : j.asList()) {
        arr.push_back(reconstructJAS(item));
      }
      return move(arr);
    } else if (j.isDict()) {
      auto& obj = j.asDict();
      auto reconstructedObject = Var::Dict{};
      for (auto& [key, val] : obj) {
        auto _jas = constructJAS(key, reconstructJAS(val));
        if (_jas.isDict()) {
          for (auto& [_newKey, _newVal] : _jas.asDict()) {
            reconstructedObject[_newKey] = _newVal;
          }
        }
      }
      return std::move(reconstructedObject);
    } else if (j.isString()) {
      return constructJAS(j.asString());
    } else {
      return j;
    }
  }

  Var constructJAS(const String& expression) {
    if (expression.empty()) {
      return expression;
    }

    if (isSpecifier(expression)) {
      return expression;
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
      if (!jexpr.isNull()) {
        return jexpr;
      }
    }
    size_t b = 0;
    size_t e = expression.size() - 1;
    increaseTillNotSpace(expression, b);
    increaseTillNotSpace(expression, e, -1);
    if ((e - b) == expression.size()) {
      return expression;
    } else if (b <= e) {
      return expression.substr(b, e + 1);
    } else {
      return {};
    }
  }

  Var constructJAS(const String& specifier, const String& expression) {
    if (isVariableLike(specifier) || isSpecifierLike(specifier)) {
      return Var::dict({{specifier, constructJAS(expression)}});
    }
    return {};
  }

  template <class Iterator>
  Var makeJAS(Iterator specifierBeg, Iterator specifierEnd,
              const Var& jevaluable) {
    if (specifierBeg != specifierEnd) {
      auto curIt = specifierBeg;
      return Var::dict({{String{*curIt},
                         makeJAS(++specifierBeg, specifierEnd, jevaluable)}});
    } else {
      return jevaluable;
    }
  }

  Var constructJAS(const String& expression, const Var& jevaluable) {
    if (expression == keyword::noeval || isSpecifier(expression)) {
      return Var::dict({{expression, jevaluable}});
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

    return Var::dict({{expression, jevaluable}});
  }

  EvaluablePtr translateEVBDict(const Var& j) {
    EvaluablePtr evb;

    do {
      if (!j.isDict()) {
        break;
      }
      auto evbDict = makeEDict();
      String id;
      StackVariables stackVariables;

      for (auto& [key, jvalue] : j.asDict()) {
        if (key == keyword::id) {
          throwIf<SyntaxError>(!jvalue.isString(),
                               JASSTR("Following id specifier `"), key,
                               JASSTR("` must be a string"));
          evbDict->id = jvalue.asString();
        } else if (auto vi = extractStackVariables(this, key, jvalue);
                   vi.has_value()) {
          stackVariables.insert(move(*vi));
        } else {
          evbDict->value.emplace(key, translateImpl(jvalue));
        }
      }

      if (!stackVariables.empty()) {
        evbDict->stackVariables =
            make_shared<StackVariables>(std::move(stackVariables));
      } else if (evbDict->id.empty() &&
                 std::all_of(
                     std::begin(evbDict->value), std::end(evbDict->value),
                     [](auto&& e) { return isType<DirectVal>(e.second); })) {
        evb = makeDV(j);
        break;
      }

      evb = evbDict;
    } while (false);
    return evb;
  }

  EvaluablePtr translateEVBList(const Var& j) {
    if (!j.isList()) {
      return {};
    }

    EvaluableList::value_type arr;
    for (auto& item : j.asList()) {
      arr.push_back(translateImpl(item));
    }

    if (std::all_of(std::begin(arr), std::end(arr),
                    [](auto&& e) { return isType<DirectVal>(e); })) {
      return makeDV(j);
    } else {
      return makeEList(move(arr));
    }
  }

  EvaluablePtr translateImpl(const Var& expr) {
    for (auto translate : translateCallbacks_) {
      if (auto evaled = (this->*translate)(expr)) {
        return evaled;
      }
    }
    return {};
  }

  void checkVersionCompatibility(const Var& jas) {
    auto exprVer = jas.getAt(JASSTR("$jas.version")).getString();
    if (!exprVer.empty()) {
      auto gt = mdl::cif::gt_ver(Var::list({exprVer, jas::version})).getBool();
      throwIf<SyntaxError>(
          gt,
          "Given JAS syntax has higher version than current JAS translater, "
          "current[",
          jas::version, "], given[", exprVer, "]");
    }
  }

  EvaluablePtr translate(const Var& jas, Translator::Strategy strategy) {
    throwIf<SyntaxError>(jas.isNull(), JASSTR("Not an Evaluable: "),
                         jas.dump());
    checkVersionCompatibility(jas);
    if (strategy == Translator::Strategy::AllowShorthand) {
      auto _jas = reconstructJAS(jas);
      return translateImpl(_jas);
    } else {
      return translateImpl(jas);
    }
  }
};  // namespace jas

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

EvaluablePtr Translator::translate(EvalContextPtr ctxt, const Var& jas,
                                   Strategy strategy) {
  impl_->context_ = move(ctxt);
  return impl_->translate(jas, strategy);
}

Var Translator::reconstructJAS(EvalContextPtr ctxt, const Var& jas) {
  impl_->context_ = move(ctxt);
  return impl_->reconstructJAS(jas);
}

const std::set<StringView>& Translator::evaluableSpecifiers() {
  return TranslatorImpl::evaluableSpecifiers();
}

Translator::Translator(ModuleManager* moduleMgr)
    : impl_(new TranslatorImpl(moduleMgr)) {}

Translator::~Translator() {
  if (impl_) {
    delete impl_;
  }
}

// namespace translater
}  // namespace jas
