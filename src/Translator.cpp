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

#define JAS_REGEX_VARIABLE_NAME R"([\.\$a-zA-Z_][a-zA-Z_0-9]*)"
#define JAS_REGEX_VARIABLE R"(\$)" JAS_REGEX_VARIABLE_NAME

struct EvaluableInfo {
  String type;
  Var params;
};

using std::make_shared;
using std::move;
using std::shared_ptr;
using OptionalEvbInfo = std::optional<EvaluableInfo>;
using Regex = std::basic_regex<CharType>;
using RegexMatchResult = std::match_results<String::const_iterator>;
using RegexTokenIterator = std::regex_token_iterator<String::const_iterator>;

static std::vector<StringView> split(StringView str, const String& delim);
struct TranslatorImpl {
  using ParsingRuleCallback =
      EvaluablePtr (TranslatorImpl::*)(Evaluable* parent, const Var&);
  using KeywordTranslatorMap = std::map<String, ParsingRuleCallback>;
  TranslatorImpl(ModuleManager* moduleMgr) : moduleMgr_(moduleMgr) {}

  ModuleManager* moduleMgr_;
  ContextPtr context_;
  std::vector<ParsingRuleCallback> translateCallbacks_ = {
      &TranslatorImpl::translateNoEffectOperations,    //
      &TranslatorImpl::translateOperations,            //
      &TranslatorImpl::translateVariableModification,  //
      &TranslatorImpl::translateEVBList,               //
      &TranslatorImpl::translateEVBDict,               //
      &TranslatorImpl::translateNoneKeywordVal,        //
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
  static bool isMacroLike(const StringView& str) {
    return !str.empty() && str[0] == prefix::macro;
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

  static bool isValidVariableName(const StringView& varname) {
    static auto varregex = Regex(JAS_REGEX_VARIABLE_NAME);
    return std::regex_match(varname.begin(), varname.end(), varregex);
  }

  bool isSpecifier(const StringView& str) const {
    return !str.empty() &&
           (isEvaluableSpecifier(str) || isNonOpSpecifier(str) ||
            functionSupported({}, str.substr(1)));
  }

  bool isJasFunction(const StringView& /*module*/,
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
      __jas_throw_if(SyntaxError, isSpecifierLike(str) && !isSpecifier(str),
                     JASSTR("Following character `"), prefix::specifier,
                     JASSTR("` must be a specifier or supported function: ??"),
                     str, JASSTR("??"));
    }
  }

  static std::optional<EvaluableInfo> extractOperationInfo(const Var& j) {
    std::optional<EvaluableInfo> evbi;
    if (j.isDict()) {
      for (auto& [key, val] : j.asDict()) {
        if (!key.empty() && key[0] == prefix::specifier) {
          evbi = EvaluableInfo{key, val};
          break;
        }
      }
    }
    return evbi;
  }

  static void translateVariableModification(TranslatorImpl* translator,
                                            UseStackEvaluable* parent,
                                            const String& key,
                                            const Var& evbExpr) {
    String::size_type namePos = 1;
    auto type = VariableEvalInfo::Declaration;
    if (key[namePos] == prefix::varupdate) {
      ++namePos;
      type = VariableEvalInfo::Update;
    }
    if (!parent->localVariables) {
      parent->localVariables = make_shared<LocalVariables>();
    }
    auto varname = key.substr(namePos);

    __jas_throw_if(SyntaxError, !isValidVariableName(varname),
                   "Invalid variable name: `", key, "`");
    parent->localVariables->emplace(
        move(varname),
        VariableEvalInfo{translator->translateImpl(parent, evbExpr), type});
  }

  static void translateMacro(TranslatorImpl* translator,
                             UseStackEvaluable* parent, const String& key,
                             const Var& evbExpr) {
    if (!parent->localMacros) {
      parent->localMacros = make_shared<LocalMacrosMap>();
    }
    parent->localMacros->emplace(key.substr(1),
                                 translator->translateImpl(parent, evbExpr));
  }

  static void extractLocalSymbols(TranslatorImpl* translator,
                                  UseStackEvaluable* currentEvb, const Var& j) {
    if (j.isDict()) {
      for (auto& [key, val] : j.asDict()) {
        if (!isMacroLike(key)) {
          continue;
        }
        translateMacro(translator, currentEvb, key, val);
      }
      for (auto& [key, val] : j.asDict()) {
        if (!(isVariableLike(key) && key.size() > 1)) {
          continue;
        }
        translateVariableModification(translator, currentEvb, key, val);
      }
    }
  }

  template <class _op_translator, class _operation_type>
  struct OperationTranslatorBase {
    using operation_type = _operation_type;
    static operation_type mapToEvbType(const String& sop) {
      if (auto it = _op_translator::s2op_map().find(sop);
          it != std::end(_op_translator::s2op_map())) {
        return it->second;
      } else {
        return operation_type::invalid;
      }
    }
  };

  template <class _op_translator, class _operation_type>
  struct OperatorTranslatorBase
      : public OperationTranslatorBase<_op_translator, _operation_type> {
    using _Base = OperationTranslatorBase<_op_translator, _operation_type>;
    static EvaluablePtr translate(TranslatorImpl* translator, Evaluable* parent,
                                  const Var& expression,
                                  const OptionalEvbInfo& evbInfo) {
      if (evbInfo) {
        auto& [sop, jevaluation] = evbInfo.value();
        if (auto op = _Base::mapToEvbType(sop);
            op != _operation_type::invalid) {
          auto evbOp = makeOp(parent, op);
          extractLocalSymbols(translator, evbOp.get(), expression);

          auto& params = evbOp->params;
          if (jevaluation.isList()) {
            for (auto& item : jevaluation.asList()) {
              params.emplace_back(translator->translateImpl(evbOp.get(), item));
            }
          } else {
            params.emplace_back(
                translator->translateImpl(evbOp.get(), jevaluation));
          }
          return evbOp;
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
      : public OperationTranslatorBase<ListOpTranslator, lsaot> {
    static const auto& s2op_map() {
      static std::map<String, lsaot> _ = {
          {keyword::any_of, lsaot::any_of},
          {keyword::all_of, lsaot::all_of},
          {keyword::none_of, lsaot::none_of},
          {keyword::count_if, lsaot::count_if},
          {keyword::filter_if, lsaot::filter_if},
          {keyword::transform, lsaot::transform},
      };
      return _;
    }

    static EvaluablePtr translate(TranslatorImpl* translator, Evaluable* parent,
                                  const Var& expression,
                                  const OptionalEvbInfo& evbInfo) {
      shared_ptr<ListAlgorithm> output;
      do {
        if (!evbInfo) {
          break;
        }
        auto& [sop, jevaluable] = evbInfo.value();
        auto op = mapToEvbType(sop);
        if (op == lsaot::invalid) {
          break;
        }

        EvaluablePtr evbCond;
        EvaluablePtr list;

        output = makeOp(parent, op);
        auto listFromCurrentContextData = [output] {
          return makeSimpleFI<ContextFI>(output.get(), JASSTR("field"));
        };
        extractLocalSymbols(translator, output.get(), expression);

        if (jevaluable.isDict()) {
          auto jcond = jevaluable.getAt(keyword::cond);
          if (jcond.isNull()) {
            jcond = jevaluable.getAt(keyword::op);
          }
          if (jcond.isNull()) {
            jcond = jevaluable;
          }
          if (!jcond.isNull()) {
            evbCond = translator->translateImpl(output.get(), jcond);
          }

          __jas_throw_if(
              SyntaxError, !evbCond, JASSTR("Follow operation `"), sop,
              JASSTR("` must be an evaluable condition with specifier `"),
              keyword::cond, JASSTR("` or `"), keyword::op, "`");

          EvaluablePtr list;
          if (auto jlistExpr = jevaluable.getAt(keyword::list);
              jlistExpr.isNull()) {
            list = listFromCurrentContextData();
          } else {
            list = translator->translateImpl(parent, jlistExpr);
          }

          output->cond = move(evbCond);
          output->list = move(list);
          break;
        }

        if (jevaluable.isBool()) {
          output->cond = makeConst(output.get(), jevaluable);
          output->list = listFromCurrentContextData();
        }

      } while (false);
      return output;
    }
  };

  EvaluablePtr translateNoneKeywordVal(Evaluable* parent, const Var& expr) {
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
            return makeConst(parent, std::stoi(realVal));
          case JASSTR('f'):
            return makeConst(parent, std::stod(realVal));
          case JASSTR('l'):
            return makeConst(parent, static_cast<int64_t>(std::stoll(realVal)));
          case JASSTR('s'):
            return makeConst(parent, std::move(realVal));
          case JASSTR('b'):
            if (realVal == JASSTR("true")) {
              return makeConst(parent, true);
            } else if (realVal == JASSTR("false")) {
              return makeConst(parent, false);
            } else {
              __jas_throw(SyntaxError, "specifier %b cannot apply to ",
                          realVal);
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
    return makeConst(parent, expr);
  }

  bool isContextFI(const StringView& module, const StringView& funcName) const {
    if (context_ && module.empty()) {
      return context_->functionSupported(funcName);
    }
    return false;
  }

  struct FunctionTranslator {
    static EvaluablePtr translate(TranslatorImpl* translator, Evaluable* parent,
                                  const Var& expr,
                                  const OptionalEvbInfo& evbInfo) {
      auto splitModule = [](const StringView& str) {
        StringView funcName{str.data() + 1, str.size() - 1};
        StringView moduleName;
        auto dotPos = funcName.find_last_of(JASSTR('.'));
        if (dotPos != StringView::npos) {
          moduleName = funcName.substr(0, dotPos);
          funcName = funcName.substr(dotPos + 1);
          __jas_throw_if(SyntaxError, moduleName.empty(),
                         "Module name must not empty `", str, "`");
          __jas_throw_if(SyntaxError, funcName.empty(),
                         "FunctionInvocation name after "
                         "module-separator(.) must not be empty `",
                         str, "`");
          return std::make_pair(move(funcName), move(moduleName));
        } else {
          return std::make_pair(move(funcName), StringView{JASSTR("")});
        }
      };

      String id;
      String funcName;
      String moduleName;
      const Var* jParamExpr = nullptr;

      auto _makeFunction = [&]() -> EvaluablePtr {
        auto _completeInvocation = [&](auto fi) {
          extractLocalSymbols(translator, fi.get(), expr);
          if (jParamExpr) {
            fi->param = translator->translateImpl(fi.get(), *jParamExpr);
          }
          return fi;
        };

        if (translator->isContextFI(moduleName, funcName)) {
          return _completeInvocation(
              makeSimpleFI<ContextFI>(parent, String(funcName)));
        } else if (translator->isJasFunction(moduleName, funcName)) {
          return _completeInvocation(
              makeSimpleFI<EvaluatorFI>(parent, String(funcName)));
        } else if (!moduleName.empty()) {
          auto module = translator->moduleMgr_->getModule(moduleName);
          __jas_throw_if(SyntaxError, !module, "Theres no module named `",
                         moduleName, "` when invoking `", moduleName, ".",
                         funcName, "`");
          __jas_throw_if(SyntaxError, !module->has(funcName),
                         "There's no function named `", funcName,
                         "` in module `", moduleName, "`");
          return _completeInvocation(
              makeModuleFI(parent, String(funcName), move(module)));
        } else {
          auto module = translator->moduleMgr_->getModule(moduleName);
          if (module && module->has(funcName)) {
            return _completeInvocation(
                makeModuleFI(parent, String(funcName), move(module)));
          } else if (auto module =
                         translator->moduleMgr_->findModuleByFuncName(funcName);
                     module) {
            return _completeInvocation(
                makeModuleFI(parent, String(funcName), move(module)));
          }
        }

        auto mcfi = _completeInvocation(makeMacroFI(parent, funcName, {}));
        // 1. if is constant-list: let it be itself
        // 2. if it is EVBList -> let it be it self
        // 3. else: let it be EVBList{itself}
        do {
          if (!mcfi->param) {
            break;
          }
          if (isType<EvaluableList>(mcfi->param)) {
            break;
          }
          if (isType<Constant>(mcfi->param)) {
            if (static_cast<const Constant*>(mcfi->param.get())
                    ->value.isList()) {
              break;
            }
          }

          mcfi->param = makeEvbList(
              mcfi.get(), EvaluableList::ValueType{move(mcfi->param)});

        } while (false);

        mcfi->macro = mcfi->resolveMacro(funcName);
        __jas_throw_if(SyntaxError, !mcfi->macro,
                       "Unknown reference to function: `@", funcName);
        return mcfi;
      };

      if (evbInfo) {
        auto& [sop, jevaluable] = evbInfo.value();
        if (isSpecifierLike(sop)) {
          std::tie(funcName, moduleName) = splitModule(sop);
          jParamExpr = &jevaluable;
          return _makeFunction();
        }
      } else if (expr.isString()) {
        auto& str = expr.asString();
        if (!str.empty() && translator->isSpecifierLike(str)) {
          std::tie(funcName, moduleName) = splitModule(str);
          return _makeFunction();
        }
      }
      return {};
    }
  };

  EvaluablePtr translateNoEffectOperations(Evaluable* parent, const Var& expr) {
    if (expr.isDict() && expr.contains(keyword::noeval)) {
      __jas_throw_if(SyntaxError, expr.size() > 1, "Object must contain only ",
                     "keyword `", keyword::noeval,
                     "` and not evaluated expression");
      return makeConst(parent, expr.at(keyword::noeval));
    }
    return {};
  }

  template <class _OperationTranslator>
  EvaluablePtr _translateOperation(Evaluable* parent, const Var& expr,
                                   const OptionalEvbInfo& evbInfo) {
    return _OperationTranslator::translate(this, parent, expr, evbInfo);
  }

  EvaluablePtr translateOperations(Evaluable* parent, const Var& expr) {
    using OperationTranslatorCallback = EvaluablePtr (TranslatorImpl::*)(
        Evaluable * parent, const Var& expr, const OptionalEvbInfo& evbInfo);
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
      if (auto evb = (this->*translateFunc)(parent, expr, evbInfo)) {
        return evb;
      }
    }
    return {};
  }

  EvaluablePtr _makeVariableLikeEvb(Evaluable* parent, String objName) {
    auto objNamePrefix = objName.front();
    if (objNamePrefix <= JASSTR('9') && objNamePrefix >= JASSTR('0')) {
      __jas_throw_if(
          SyntaxError,
          objNamePrefix == JASSTR('0') ||
              !std::all_of(objName.begin() + 1, objName.end(), ::isdigit),
          "Variable name must not prefix by a number: `$", objName, "`");
      // for case of lookup function's argument
      auto argPos = std::atoi(objName.data());
      __jas_throw_if(SyntaxError, argPos > std::numeric_limits<uint8_t>::max(),
                     "Exeeds limit functions arguments index: `$", objName,
                     "`");
      return makeCtxtArg(parent, static_cast<uint8_t>(argPos));
    } else if (objNamePrefix == prefix::argc) {
      __jas_throw_if(SyntaxError, objName.size() > 1,
                     "Invalid context argument count symbol: `$", objName, "`");
      return makeCtxtArgInfo(parent, ContextArgumentsInfo::Type::ArgCount);
    } else if (objNamePrefix == prefix::args) {
      __jas_throw_if(SyntaxError, objName.size() > 1,
                     "Invalid all_context_arguments symbol: `$", objName, "`");
      return makeCtxtArgInfo(parent, ContextArgumentsInfo::Type::Args);
    } else {
      using namespace std;
      static const char PRESERVED_CHARS[] = JASSTR("!`+-*&^%$#@/\\';,=");
      auto prefixedWithPreservedChar =
          std::find(begin(PRESERVED_CHARS), end(PRESERVED_CHARS),
                    objNamePrefix) != end(PRESERVED_CHARS) &&
          objNamePrefix != prefix::variable;

      __jas_throw_if(
          SyntaxError, prefixedWithPreservedChar,
          "Variable is not allowed to prefix with following letter:{",
          PRESERVED_CHARS, "}: `$", objName, "`");

      return makeVariable(parent, std::move(objName));
    }
  }

  EvaluablePtr translateVariableModification(Evaluable* parent, const Var& j) {
    EvaluablePtr evb;
    do {
      if (!j.isString()) {
        break;
      }
      auto& expression = j.asString();
      if (!isVariableLike(expression)) {
        break;
      }
      if (evb = _translateVariablePropertyQuery(parent, expression); evb) {
        break;
      }
      if (expression.find(JASSTR(':')) != String::npos) {
        if (evb = this->translateImpl(parent, _constructJAS(expression, false));
            evb) {
          break;
        }
      }
      auto varName = expression.substr(1);
      __jas_throw_if(SyntaxError, varName.empty(),
                     "Variable name must not be empty");
      return _makeVariableLikeEvb(parent, std::move(varName));
    } while (false);
    return evb;
  }

  EvaluablePtr _translateVariablePropertyQuery(Evaluable* parent,
                                               const String& expression) {
    using namespace std;
    static const Regex rgxVariablePropertyQuery{JASSTR(
        R"_regex(\$(\.?\*?[a-zA-Z_0-9]*)\[((?:\$[a-zA-Z_]+[0-9]*|[@:a-zA-Z_0-9\-]+|[\/]*)+)\])_regex")};
    static const Regex rgxField{JASSTR(R"_regex([^\/]+)_regex")};

    assert(isVariableLike(expression));
    EvaluablePtr evb;
    do {
      RegexMatchResult matches;
      if (!regex_match(expression, matches, rgxVariablePropertyQuery) ||
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
            jsubExpr = this->_constructJAS(subExpr);
          } else {
            jsubExpr = subExpr;
          }

          path.push_back(translateImpl(parent, jsubExpr));
        }
        ++fieldIt;
      }
      evb = makeVariableFieldQuery(
          parent, _makeVariableLikeEvb(parent, move(varName)), move(path));
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
      return _constructJAS(j.asString());
    } else {
      return j;
    }
  }

  Var _constructJAS(const StringView& expression, bool ignoreVariable = true) {
    if (expression.empty()) {
      return String{expression};
    }

    if (isVariableLike(expression) && ignoreVariable) {
      return String{expression};
    }

    auto increaseTillNotSpace = [](const StringView& s, size_t& currentPos,
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
      ++nextSpecifierPos;
      __jas_throw_if(SyntaxError, nextSpecifierPos >= expression.size(),
                     "Invalid syntax: `", expression, "`");
      increaseTillNotSpace(expression, thisSpecifierEndPos, -1);
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
      return String{expression};
    } else if (b <= e) {
      return String{expression.substr(b, e + 1)};
    } else {
      return {};
    }
  }

  Var constructJAS(const StringView& specifier, const StringView& expression) {
    if (isVariableLike(specifier) || isSpecifierLike(specifier) ||
        isVariableLike(expression) || isSpecifierLike(expression)) {
      return Var::dict({{String{specifier}, _constructJAS(expression)}});
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

  Var constructJAS(const StringView& expression, const Var& jevaluable) {
    if (expression == keyword::noeval || isSpecifier(expression)) {
      return Var::dict({{String{expression}, jevaluable}});
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

    return Var::dict({{String{expression}, jevaluable}});
  }

  EvaluablePtr translateEVBDict(Evaluable* parent, const Var& j) {
    EvaluablePtr evb;

    do {
      if (!j.isDict()) {
        break;
      }
      auto evbDict = makeEvbDict(parent);
      String id;
      LocalVariables stackVariables;
      extractLocalSymbols(this, evbDict.get(), j);
      for (auto& [key, jvalue] : j.asDict()) {
        if (!isVariableLike(key) && !isMacroLike(key)) {
          evbDict->value.emplace(key, translateImpl(evbDict.get(), jvalue));
        }
      }

      if (!evbDict->hasLocalSymbols() &&
          std::all_of(std::begin(evbDict->value), std::end(evbDict->value),
                      [](auto&& e) { return isType<Constant>(e.second); })) {
        evb = makeConst(parent, j);
        break;
      }

      evb = evbDict;
    } while (false);
    return evb;
  }

  EvaluablePtr translateEVBList(Evaluable* parent, const Var& j) {
    if (!j.isList()) {
      return {};
    }

    auto evbList = makeEvbList(parent);
    for (auto& item : j.asList()) {
      evbList->value.push_back(translateImpl(evbList.get(), item));
    }

    if (std::all_of(std::begin(evbList->value), std::end(evbList->value),
                    [](auto&& e) { return isType<Constant>(e); })) {
      return makeConst(parent, j);
    } else {
      return evbList;
    }
  }

  EvaluablePtr translateImpl(Evaluable* parent, const Var& expr) {
    for (auto translate : translateCallbacks_) {
      if (auto evb = (this->*translate)(parent, expr)) {
        return evb;
      }
    }
    return {};
  }

  EvaluablePtr translate(const Var& jas, Translator::Strategy strategy) {
    __jas_throw_if(SyntaxError, jas.isNull(), JASSTR("Not an Evaluable: "),
                   jas.dump());
    if (strategy == Translator::Strategy::AllowShorthand) {
      auto _jas = reconstructJAS(jas);
      return translateImpl(nullptr, _jas);
    } else {
      return translateImpl(nullptr, jas);
    }
  }
};

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

Var Translator::reconstructJAS(EvalContextPtr ctxt, const Var& script) {
  impl_->context_ = move(ctxt);
  if (script.isDict() && script.contains(version_expression_key)) {
    auto scriptVersion = script.at(version_expression_key);
    __jas_throw_if(SyntaxError,
                   mdl::cif::gt_ver(Var::List{script.at(version_expression_key),
                                              jas::version}),
                   "Script version is higher than current jas::version: ",
                   scriptVersion.dump(), " > ", jas::version);
  }
  return impl_->reconstructJAS(script);
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

// namespace translator
}  // namespace jas
