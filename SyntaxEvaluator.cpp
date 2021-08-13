#include "SyntaxEvaluator.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iomanip>
#include <list>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>

#include "CIF.h"
#include "EvaluableClasses.h"
#include "Exception.h"
#include "Keywords.h"
#include "String.h"
#include "SyntaxValidator.h"
#include "jaop.h"

#define lambda_on_this(method, ...) [&] { return method(__VA_ARGS__); }

namespace jas {
using std::make_shared;
using std::move;
using EvaluatedValues = std::vector<EvaluatedValue>;

template <class _Exception>
struct StackUnwin : public _Exception {
  using _Base = _Exception;
  using _Base::_Base;
};

template <class _Callable>
struct ExitCall {
  ExitCall(_Callable&& _cb) : cb{std::forward<_Callable>(_cb)} {}
  ~ExitCall() { cb(); }
  _Callable cb;
};

Json toJson(const JAdapterPtr& jap) {
  if (jap) {
    return *jap;
  } else {
    return {};
  }
}

auto makeEvaluatedVal(JsonAdapter value) {
  return make_shared<JsonAdapter>(move(value));
}

template <class _Callable>
auto callWhenExit(_Callable&& cb) {
  return ExitCall<_Callable>(std::forward<_Callable>(cb));
}

template <class _Callable>
EvaluatedValue filter_if(const JsonArray& list, _Callable&& cond) {
  JsonArray filtered;
  for (auto& item : list) {
    if (cond(item)) {
      JsonTrait::add(filtered, item);
    }
  }
  return makeEvaluatedVal(move(filtered));
}

template <class _Callable>
EvaluatedValue transform(const JsonArray& list, _Callable&& transf) {
  JsonArray transformed;
  for (auto& item : list) {
    JsonTrait::add(transformed, toJson(transf(item)));
  }
  return makeEvaluatedVal(move(transformed));
}

class SyntaxEvaluatorImpl : public EvaluatorBase {
 public:
  EvaluatedValue evaluate(const Evaluable& e,
                          EvalContextPtr rootContext = nullptr);
  EvaluatedValue evaluate(const EvaluablePtr& e,
                          EvalContextPtr rootContext = nullptr);

  void setDebugInfoCallback(DebugOutputCallback);

  ~SyntaxEvaluatorImpl();

  struct EvaluationFrame;
  struct EvaluatedOnReadValue;
  using EvaluationStack = std::list<EvaluationFrame>;
  using EvaluatedOnReadValues = std::vector<EvaluatedOnReadValue>;

  struct EvaluationFrame {
    EvaluationFrame(EvalContextPtr _context = {},
                    EvaluatedValue _returnedValue = {},
                    const Evaluable* _evb = nullptr)
        : context(_context), returnedValue(_returnedValue), evb(_evb) {}

    EvalContextPtr context;
    EvaluatedValue returnedValue;
    const Evaluable* evb;
    StackVariablesPtr variables;
  };

  struct EvaluatedOnReadValue {
    EvaluatedOnReadValue(SyntaxEvaluatorImpl* delegate, const Evaluable& e)
        : delegate_(delegate), e_(e) {}

    operator EvaluatedValue() const { return delegate_->_evalRet(&e_); }

    SyntaxEvaluatorImpl* delegate_;
    const Evaluable& e_;
  };

  EvaluationStack stack_;
  DebugOutputCallback dbCallback_;
  int evalCount_ = 0;
  //-----------------------------------------------
  void eval(const DirectVal& v) override;
  void eval(const EvaluableMap& v) override;
  void eval(const EvaluableArray& v) override;
  void eval(const ArithmaticalOperator& op) override;
  void eval(const LogicalOperator& op) override;
  void eval(const ComparisonOperator& op) override;
  void eval(const ListOperation& op) override;
  void eval(const Function& func) override;
  void eval(const VariableFieldQuery& query) override;
  void eval(const Variable& rv) override;
  EvaluatedValue _queryOrEvalVariable(const String& variableName);
  void _evalOnStack(const Evaluable* e, String ctxtID = {},
                    JsonAdapter ctxtData = {});
  EvaluatedValue _evalRet(const Evaluable* e, String ctxtID = {},
                          JsonAdapter ctxtData = {});
  /// Exceptions
  template <class _Exception, typename... _Msg>
  void evalThrowIf(bool cond, _Msg&&... msg);
  template <class _Exception, typename... _Msg>
  void evalThrow(_Msg&&...);

  /// Stack functions
  String stackDump() const;
  void stackPush(String ctxtID, const Evaluable* evb,
                 JsonAdapter contextData = {});
  void stackPop();
  void stackReturn(JsonAdapter val);
  void stackReturn(EvaluatedValue&& val);
  void stackReturn(EvaluatedValue&& val, const Evaluable& ev);
  EvaluatedValue& stackReturnedVal();
  EvaluatedValue stackTakeReturnedVal();
  EvaluationFrame& stackTopFrame();
  const EvalContextPtr& stackTopContext();
  void evaluateVariables(const StackVariablesPtr& params);
  String generateBackTrace(const String& msg) const;
  String dump(const Evaluables& l);

  /// Evaluations:
  template <class _Operator, class _Callable>
  EvaluatedValue evaluateOperator(const _Operator& op, _Callable&& eval_func);
  template <class _ComparisonMethod, size_t expected_count, class _Operation>
  void validateParamCount(const _Operation& o);
  template <class _Operation>
  void makeSureUnaryOp(const _Operation& o);
  template <class T>
  void makeSureBinaryOp(const _OperatorBase<T, typename T::OperatorType>& o);
  template <class T>
  void makeSureSingleBinaryOp(
      const _OperatorBase<T, typename T::OperatorType>& o);
  template <class _OpType, _OpType _opType, template <class> class _std_op,
            template <class, class> class _ApplierImpl>
  EvaluatedValue applyOp(const EvaluatedOnReadValues& ievals);
  template <class _OpType, _OpType _opType, template <class> class _std_op>
  EvaluatedValue applySingleBinOp(const EvaluatedOnReadValues& ievals);
  template <class _OpType, _OpType _opType, template <class> class _std_op>
  EvaluatedValue applyMultiBinOp(const EvaluatedOnReadValues& ievals);

  template <class _OpType, _OpType _opType, template <class> class _std_op>
  EvaluatedValue applyUnaryOp(const EvaluatedOnReadValues& ievals);

  EvaluatedValue applyLogicalAndOp(const EvaluatedOnReadValues& ievals);
  EvaluatedValue applyLogicalOrOp(const EvaluatedOnReadValues& ievals);

  static String syntaxOf(const Evaluable& e);

  template <class T, class _std_op>
  struct ApplyUnaryOpImpl {
    static EvaluatedValue apply(const EvaluatedOnReadValues& evals) {
      auto evaled = EvaluatedValue{evals.back()};
      throwIf<EvaluationError>(!evaled, "Evaluated to null");
      return makeEvaluatedVal(
          _std_op{}(EvaluatedValue { evals.back() } -> get<T>()));
    }
  };

  template <class T, class _std_op>
  struct ApplySingleBinOpImpl {
    static EvaluatedValue apply(const EvaluatedOnReadValues& evals) {
      EvaluatedValue lhsEv = evals.front();
      EvaluatedValue rhsEv = evals.back();
      try {
        return makeEvaluatedVal(_std_op{}(lhsEv->get<T>(), rhsEv->get<T>()));
      } catch (const DirectVal::TypeError&) {
        throw_<DirectVal::TypeError>("(", lhsEv->dump(), ", ", rhsEv->dump(),
                                     ")");
        return {};
      }
    }
  };

  template <class T, class _std_op>
  struct ApplyMutiBinOpImpl {
    static EvaluatedValue apply(const EvaluatedOnReadValues& evals) {
      auto it = std::begin(evals);
      EvaluatedValue lastEv = *it++;
      auto v = lastEv->get<T>();
      _std_op applier;
      while (it != std::end(evals)) {
        EvaluatedValue nextEv = *it;
        throwIf<EvaluationError>(!nextEv, "operate on null");
        try {
          v = applier(v, nextEv->get<T>());
          std::swap(lastEv, nextEv);
        } catch (const DirectVal::TypeError&) {
          throw_<DirectVal::TypeError>("(", lastEv->dump(), ", ",
                                       nextEv->dump(), ")");
        }
        ++it;
      }
      return makeEvaluatedVal(move(v));
    }
  };

  template <class T, class _std_op, bool untilVal>
  struct ApplyLogicalOpImpl {
    static EvaluatedValue apply(const EvaluatedOnReadValues& evals) {
      bool v = !untilVal;
      _std_op applier;
      for (auto& e : evals) {
        EvaluatedValue nextEv = e;
        try {
          v = applier(v, static_cast<bool>(nextEv->get<T>()));
          if (v == untilVal) {
            break;
          }
        } catch (const DirectVal::TypeError&) {
          throw_<DirectVal::TypeError>("(", v, ", ", nextEv->dump(), ")");
        }
      }
      return makeEvaluatedVal(move(v));
    }
  };
  template <class T, class _std_op>
  using ApplyLogicalAndOpImpl = ApplyLogicalOpImpl<T, _std_op, false>;
  template <class T, class _std_op>
  using ApplyLogicalOrOpImpl = ApplyLogicalOpImpl<T, _std_op, true>;
};

namespace _type_traits {
template <class T, typename = bool>
struct OperatorsSupported;

using OperatorsSupportedBits = uint32_t;
static inline constexpr OperatorsSupportedBits __SUPPORT_ALL = 0xFFFFFFFF;
static inline constexpr OperatorsSupportedBits __SUPPORT_NONE = 0;

template <class... _OpTypes>
inline constexpr OperatorsSupportedBits __supportOps(_OpTypes... ops) {
  return ((1 << static_cast<OperatorsSupportedBits>(ops)) | ...);
}

template <OperatorsSupportedBits SUPPORTED_AR_LIST = __SUPPORT_ALL,
          OperatorsSupportedBits SUPPORTED_LG_LIST = __SUPPORT_ALL,
          OperatorsSupportedBits SUPPORTED_CMP_LIST = __SUPPORT_ALL>
struct OperatorsSupportedArLgCmpImpl {
  template <class _optype>
  static constexpr OperatorsSupportedBits __2bits(_optype opt) {
    return 1 << static_cast<OperatorsSupportedBits>(opt);
  }
  static constexpr bool supported(ArithmeticOperatorType opt) {
    return SUPPORTED_AR_LIST & __2bits(opt);
  }
  static constexpr bool supported(LogicalOperatorType opt) {
    return SUPPORTED_LG_LIST & __2bits(opt);
  }
  static constexpr bool supported(ComparisonOperatorType opt) {
    return SUPPORTED_CMP_LIST & __2bits(opt);
  }
};

template <>
struct OperatorsSupported<String>
    : OperatorsSupportedArLgCmpImpl<__supportOps(ArithmeticOperatorType::plus),
                                    __SUPPORT_NONE, __SUPPORT_ALL> {};

template <>
struct OperatorsSupported<bool>
    : OperatorsSupportedArLgCmpImpl<__SUPPORT_NONE, __SUPPORT_ALL,
                                    __SUPPORT_ALL> {};

template <typename T>
struct OperatorsSupported<T, std::enable_if_t<std::is_integral_v<T>, bool>>
    : OperatorsSupportedArLgCmpImpl<> {};

template <typename T>
struct OperatorsSupported<T,
                          std::enable_if_t<std::is_floating_point_v<T>, bool>>
    : OperatorsSupportedArLgCmpImpl<__supportOps(
                                        ArithmeticOperatorType::plus,
                                        ArithmeticOperatorType::minus,
                                        ArithmeticOperatorType::multiplies,
                                        ArithmeticOperatorType::divides),
                                    __SUPPORT_ALL, __SUPPORT_ALL> {};

template <class T, typename>
struct OperatorsSupported
    : OperatorsSupportedArLgCmpImpl<__SUPPORT_NONE, __SUPPORT_NONE,
                                    __SUPPORT_NONE> {};

template <class T, class _OpType>
constexpr bool operationSupported(_OpType op) {
  return OperatorsSupported<T>::supported(op);
}
}  // namespace _type_traits

template <class _StdOperator>
static JsonAdapter binaryOperationAccumulate(
    const SyntaxEvaluatorImpl::EvaluatedOnReadValues& evaluatedVals) {
  throwIf<EvaluationError>(evaluatedVals.size() < 2,
                           "Size must be greator than 2");

  auto op = _StdOperator{};
  auto it = std::begin(evaluatedVals);
  EvaluatedValue e = *it;
  throwIf<EvaluationError>(!e, "Evaluated to null");
  JsonAdapter result = *e;
  while (++it != std::end(evaluatedVals)) {
    e = *it;
    throwIf<EvaluationError>(!e, "Evaluated to null");
    result = op(result, *e);
  }
  return result;
}

#define __MC_STACK_START(ctxtID, evb, ...) \
  stackPush(ctxtID, evb, ##__VA_ARGS__);   \
  try {
#define __MC_STACK_END                              \
  }                                                 \
  catch (const StackUnwin<SyntaxError>& e) {        \
    throw e;                                        \
  }                                                 \
  catch (const StackUnwin<EvaluationError>& e) {    \
    throw e;                                        \
  }                                                 \
  catch (const StackUnwin<jas::Exception>& e) {     \
    throw e;                                        \
  }                                                 \
  catch (jas::Exception & e) {                      \
    evalThrow<jas::Exception>(e.details);           \
  }                                                 \
  catch (std::exception & e) {                      \
    evalThrow<jas::Exception>(e.what());            \
  }                                                 \
  catch (...) {                                     \
    evalThrow<jas::Exception>("Unknown exception"); \
  }

#define __MC_STACK_END_RETURN(ret) __MC_STACK_END return ret;

#define __MC_BASIC_OPERATION_EVAL_START(op) try {
#define __MC_BASIC_OPERATION_EVAL_END(op)                           \
  }                                                                 \
  catch (const DirectVal::TypeError& e) {                           \
    evalThrow<EvaluationError>(                                     \
        "Evaluation Error: ", SyntaxValidator{}.generateSyntax(op), \
        " -> parameters of operation `", op.type,                   \
        "` must be same type and NOT null: ", e.details);           \
  }
#define __MC_BASIC_OPERATION_EVAL_END_RETURN(op, defaultRet) \
  __MC_BASIC_OPERATION_EVAL_END(op) return defaultRet;

/// Exceptions
template <class _Exception, typename... _Msg>
void SyntaxEvaluatorImpl::evalThrowIf(bool cond, _Msg&&... msg) {
  if (cond) {
    throw_<StackUnwin<_Exception>>(generateBackTrace(strJoin(msg...)));
  }
}

template <class _Exception, typename... _Msg>
void SyntaxEvaluatorImpl::evalThrow(_Msg&&... msg) {
  throw_<StackUnwin<_Exception>>(generateBackTrace(strJoin(msg...)));
}

EvaluatedValue SyntaxEvaluatorImpl::evaluate(const Evaluable& e,
                                             EvalContextPtr rootContext) {
  SyntaxValidator validator;
  EvaluatedValue evaluated;
  if (validator.validate(e)) {
    if (!stack_.empty()) {
      stack_.clear();
    }
    // push root context to stack as main entry
    stack_.emplace_back(move(rootContext));
    _evalOnStack(&e);
    evaluated = stackTakeReturnedVal();
  } else {
    throw_<SyntaxError>(validator.getReport());
  }
  return evaluated;
}

EvaluatedValue SyntaxEvaluatorImpl::evaluate(const EvaluablePtr& e,
                                             EvalContextPtr rootContext) {
  if (e) {
    return evaluate(*e, move(rootContext));
  } else {
    return {};
  }
}

void SyntaxEvaluatorImpl::setDebugInfoCallback(DebugOutputCallback cb) {
  dbCallback_ = move(cb);
}

SyntaxEvaluatorImpl::~SyntaxEvaluatorImpl() {
  while (!stack_.empty()) {
    stack_.pop_back();
  }
}

void SyntaxEvaluatorImpl::eval(const DirectVal& v) {
  stackReturn(static_cast<const JsonAdapter&>(v));
}

void SyntaxEvaluatorImpl::eval(const EvaluableMap& v) {
  JsonObject evaluated;
  if (!v.id.empty()) {
    JsonTrait::add(evaluated, String{keyword::id}, v.id);
  }

  evaluateVariables(v.stackVariables);

  for (auto& [key, val] : v.value) {
    evaluated.emplace(key, toJson(_evalRet(val.get(), key)));
  }
  stackReturn(makeEvaluatedVal(evaluated), v);
}

void SyntaxEvaluatorImpl::eval(const EvaluableArray& v) {
  JsonArray evaluated;
  auto itemIdx = 0;
  for (auto& val : v.value) {
    try {
      evaluated.emplace_back(toJson(_evalRet(val.get(), strJoin(itemIdx))));
    } catch (const EvaluationError&) {
      evaluated.emplace_back(Json{});
    }
  }
  stackReturn(move(evaluated));
}

void SyntaxEvaluatorImpl::eval(const ArithmaticalOperator& op) {
  if ((op.type == ArithmeticOperatorType::bit_not) ||
      (op.type == ArithmeticOperatorType::negate)) {
    makeSureUnaryOp(op);
  } else {
    makeSureBinaryOp(op);
  }

  auto e = evaluateOperator(op, [&op, this](auto&& evaluatedVals) {
    __MC_BASIC_OPERATION_EVAL_START(op)
    switch (op.type) {
      case ArithmeticOperatorType::bit_and:
        return applyMultiBinOp<ArithmeticOperatorType,
                               ArithmeticOperatorType::bit_and, std::bit_and>(
            evaluatedVals);
      case ArithmeticOperatorType::bit_not:
        return applyUnaryOp<ArithmeticOperatorType,
                            ArithmeticOperatorType::bit_not, std::bit_not>(
            evaluatedVals);
      case ArithmeticOperatorType::bit_or:
        return applyMultiBinOp<ArithmeticOperatorType,
                               ArithmeticOperatorType::bit_or, std::bit_or>(
            evaluatedVals);
      case ArithmeticOperatorType::bit_xor:
        return applyMultiBinOp<ArithmeticOperatorType,
                               ArithmeticOperatorType::bit_xor, std::bit_xor>(
            evaluatedVals);
      case ArithmeticOperatorType::modulus:
        return applyMultiBinOp<ArithmeticOperatorType,
                               ArithmeticOperatorType::modulus, std::modulus>(
            evaluatedVals);
      case ArithmeticOperatorType::divides:
        return makeEvaluatedVal(
            binaryOperationAccumulate<std::divides<>>(evaluatedVals));
      case ArithmeticOperatorType::minus:
        return makeEvaluatedVal(
            binaryOperationAccumulate<std::minus<>>(evaluatedVals));
      case ArithmeticOperatorType::multiplies:
        return makeEvaluatedVal(
            binaryOperationAccumulate<std::multiplies<>>(evaluatedVals));
      case ArithmeticOperatorType::negate:
        return applyUnaryOp<ArithmeticOperatorType,
                            ArithmeticOperatorType::negate, std::negate>(
            evaluatedVals);
      case ArithmeticOperatorType::plus:
        return makeEvaluatedVal(
            binaryOperationAccumulate<std::plus<>>(evaluatedVals));
      default:
        return EvaluatedValue{};
    }
    __MC_BASIC_OPERATION_EVAL_END_RETURN(op, EvaluatedValue{})
  });

  stackReturn(move(e), op);
}

void SyntaxEvaluatorImpl::eval(const LogicalOperator& op) {
  if (op.type == LogicalOperatorType::logical_not) {
    makeSureUnaryOp(op);
  } else {
    makeSureBinaryOp(op);
  }

  auto e = evaluateOperator(op, [&op, this](auto&& evaluatedVals) {
    __MC_BASIC_OPERATION_EVAL_START(op)
    switch (op.type) {
      case LogicalOperatorType::logical_and:
        return applyLogicalAndOp(evaluatedVals);
      case LogicalOperatorType::logical_or:
        return applyLogicalOrOp(evaluatedVals);
      case LogicalOperatorType::logical_not:
        return applyUnaryOp<LogicalOperatorType,
                            LogicalOperatorType::logical_not, std::logical_not>(
            evaluatedVals);
      default:
        return EvaluatedValue{};
    }
    __MC_BASIC_OPERATION_EVAL_END_RETURN(op, EvaluatedValue{})
  });
  stackReturn(move(e), op);
}

void SyntaxEvaluatorImpl::eval(const ComparisonOperator& op) {
  makeSureSingleBinaryOp(op);
  auto e = evaluateOperator(op, [&op, this](auto&& evaluated) {
    __MC_BASIC_OPERATION_EVAL_START(op)
    auto __applyOp = [&](auto&& stdop) {
      EvaluatedValue first = evaluated.front();
      EvaluatedValue second = evaluated.back();
      throwIf<EvaluationError>(
          !first || !second, JASSTR("Compare with null: ("),
          (first ? first->dump() : JASSTR("null")), JASSTR(", "),
          (second ? second->dump() : JASSTR("null")), JASSTR(")"));

      return makeEvaluatedVal(stdop(*first, *second));
    };

    switch (op.type) {
      case ComparisonOperatorType::eq:
        return __applyOp(std::equal_to{});
      case ComparisonOperatorType::gt:
        return __applyOp(std::greater{});
      case ComparisonOperatorType::ge:
        return __applyOp(std::greater_equal{});
      case ComparisonOperatorType::lt:
        return __applyOp(std::less{});
      case ComparisonOperatorType::le:
        return __applyOp(std::less_equal{});
      case ComparisonOperatorType::neq:
        return __applyOp(std::not_equal_to{});
      default:
        return EvaluatedValue{};
    }
    __MC_BASIC_OPERATION_EVAL_END_RETURN(op, EvaluatedValue{})
  });
  stackReturn(move(e), op);
}

void SyntaxEvaluatorImpl::eval(const ListOperation& op) {
  EvaluatedValue finalEvaled;
  EvaluatedValue vlist;

  evaluateVariables(op.stackVariables);

  if (op.list) {
    vlist = _evalRet(op.list.get());
  }

  evalThrowIf<EvaluationError>(
      !vlist || !vlist->isType<JsonArray>(),
      "`@list` input of list operation was not evaluated to array type");

  auto list = vlist->get<JsonArray>();
  int itemIdx = 0;
  auto eval_impl = [this, &itemIdx, &op](const Json& data) {
    auto evaluated = _evalRet(op.cond.get(), strJoin(itemIdx++), data);
    evalThrowIf<EvaluationError>(
        !evaluated->isType<bool>(),
        "Invalid param type > operation: ", syntaxOf(op),
        "` > expected: `boolean` > real_val: `", evaluated->dump(), "`");
    return evaluated->get<bool>();
  };

  switch (op.type) {
    case ListOperationType::any_of:
      finalEvaled = makeEvaluatedVal(
          std::any_of(std::begin(list), std::end(list), eval_impl));
      break;
    case ListOperationType::all_of:
      finalEvaled = makeEvaluatedVal(
          std::all_of(std::begin(list), std::end(list), eval_impl));
      break;
    case ListOperationType::none_of:
      finalEvaled = makeEvaluatedVal(
          std::none_of(std::begin(list), std::end(list), eval_impl));
      break;
    case ListOperationType::count_if:
      finalEvaled = makeEvaluatedVal(
          std::count_if(std::begin(list), std::end(list), eval_impl));
      break;
    case ListOperationType::filter_if:
      finalEvaled = filter_if(list, eval_impl);
      break;
    case ListOperationType::transform:
      finalEvaled = transform(list, [this, &itemIdx, &op](const Json& data) {
        return _evalRet(op.cond.get(), strJoin(itemIdx++), data);
      });
      break;
    default:
      break;
  }
  stackReturn(move(finalEvaled), op);
}

void SyntaxEvaluatorImpl::eval(const Function& func) {
  evalThrowIf<SyntaxError>(func.name.empty(),
                           "Function name must not be empty");
  JsonAdapter funcRet;
  evaluateVariables(func.stackVariables);
  auto evaledParam = toJson(_evalRet(func.param.get()));
  try {
    funcRet = cif::invoke(func.name, evaledParam);
  } catch (const FunctionNotFoundError&) {
    funcRet = stackTopContext()->invoke(func.name, evaledParam);
  }
  stackReturn(makeEvaluatedVal(move(funcRet)), func);
}

void SyntaxEvaluatorImpl::eval(const VariableFieldQuery& query) {
  auto& varname = query.id;
  auto var = _queryOrEvalVariable(varname);
  do {
    if (!var || var->isNull()) {
      stackReturn(EvaluatedValue{});
      break;
    }

    JsonTrait::Path path;
    for (auto& evbField : query.field_path) {
      auto field = _evalRet(evbField.get());
      evalThrowIf<EvaluationError>(!field->isType<String>(),
                                   "Cannot evaluated to a valid path");
      path /= field->get<String>();
    }

    stackReturn(JsonTrait::get(var->value, path));
  } while (false);
}

void SyntaxEvaluatorImpl::eval(const Variable& prop) {
  stackReturn(_queryOrEvalVariable(prop.id));
}

EvaluatedValue SyntaxEvaluatorImpl::_queryOrEvalVariable(
    const String& variableName) {
  EvaluatedValue val = stackTopContext()->property(variableName);
  if (!val) {
    // loop through stack to find the not evaluated property
    for (auto it = std::rbegin(stack_); it != std::rend(stack_); ++it) {
      if (!it->variables) {
        continue;
      }
      if (auto itProp = it->variables->find(variableName);
          itProp != std::end(*it->variables)) {
        auto& varInfo = itProp->second;
        evalThrowIf<EvaluationError>(varInfo.state == VariableInfo::Evaluating,
                                     "Cyclic reference detected on variable: $",
                                     variableName);
        assert(varInfo.state == VariableInfo::NotEvaluated &&
               "Variable found in stack must have not been evaluated");
        varInfo.state = VariableInfo::Evaluating;
        val = _evalRet(varInfo.variable.get());
        varInfo.state = VariableInfo::Evaluated;
        it->context->setProperty(variableName, val);
        break;
      }
    }
    evalThrowIf<EvaluationError>(!val, "variable not found: $", variableName);
  }
  return val;
}

void SyntaxEvaluatorImpl::_evalOnStack(const Evaluable* e, String ctxtID,
                                       JsonAdapter ctxtData) {
  assert(e);
  __MC_STACK_START(move(ctxtID), e, std::move(ctxtData));
  e->accept(this);
  __MC_STACK_END
}

EvaluatedValue jas::SyntaxEvaluatorImpl::_evalRet(const Evaluable* e,
                                                  String ctxtID,
                                                  JsonAdapter ctxtData) {
  EvaluatedValue evaluated;
  if (e) {
    if (e->useStack()) {
      _evalOnStack(e, move(ctxtID), move(ctxtData));
      evaluated = stackTakeReturnedVal();
    } else {
      e->accept(this);
      swap(evaluated, stackReturnedVal());
    }
  }
  return evaluated;
}

String SyntaxEvaluatorImpl::stackDump() const {
  OStringStream oss;
  oss << std::boolalpha;
  int i = static_cast<int>(stack_.size());
  for (auto iframe = std::rbegin(stack_); iframe != std::rend(stack_);
       ++iframe) {
    oss << "#" << std::left << std::setw(3) << i-- << ": " << std::setw(60)
        << (iframe->evb ? syntaxOf(*iframe->evb)
                        : JASSTR("----------##--------"));
    if (auto ctxinfo = iframe->context->debugInfo(); !ctxinfo.empty()) {
      oss << " --> [ctxt]: " << iframe->context->debugInfo();
    }
    oss << "\n";
  }
  return oss.str();
}

void SyntaxEvaluatorImpl::stackPush(String ctxtID, const Evaluable* evb,
                                    JsonAdapter contextData) {
  stack_.push_back(
      EvaluationFrame{stackTopContext()->subContext(ctxtID, move(contextData)),
                      EvaluatedValue{}, evb});
}

void SyntaxEvaluatorImpl::stackReturn(JsonAdapter val) {
  stackReturn(makeEvaluatedVal(move(val)));
}

void SyntaxEvaluatorImpl::stackReturn(EvaluatedValue&& val) {
  stackTopFrame().returnedValue = move(val);
}

void SyntaxEvaluatorImpl::stackReturn(EvaluatedValue&& val,
                                      const Evaluable& ev) {
  if (!ev.id.empty()) {
    stackTopContext()->setProperty(ev.id, val);
  }
  stackReturn(move(val));
}

EvaluatedValue& SyntaxEvaluatorImpl::stackReturnedVal() {
  return stackTopFrame().returnedValue;
}

EvaluatedValue jas::SyntaxEvaluatorImpl::stackTakeReturnedVal() {
  using namespace std;
  EvaluatedValue e = move(stackReturnedVal());
  if (dbCallback_ && stackTopFrame().evb) {
    auto syntax = syntaxOf(*stackTopFrame().evb);
    if (syntax.size() > 60) {
      dbCallback_(strJoin(std::setw(3), ++evalCount_, JASSTR(". "), syntax,
                          JASSTR("\n\t\t`--> "),
                          (e ? e->dump() : JASSTR("null"))));
    } else {
      dbCallback_(strJoin(std::setw(3), ++evalCount_, JASSTR(". "), std::left,
                          std::setw(60), syntax, JASSTR(": "),
                          (e ? e->dump() : JASSTR("null"))));
    }
  }
  stackPop();
  return e;
}

void SyntaxEvaluatorImpl::stackPop() { stack_.pop_back(); }

SyntaxEvaluatorImpl::EvaluationFrame& SyntaxEvaluatorImpl::stackTopFrame() {
  return stack_.back();
}

const EvalContextPtr& SyntaxEvaluatorImpl::stackTopContext() {
  return stack_.back().context;
}

void SyntaxEvaluatorImpl::evaluateVariables(
    const StackVariablesPtr& variables) {
  if (variables) {
    auto ctxt = stackTopContext();
    // some variables might refer to variable in same stack frame
    stackTopFrame().variables = variables;
    for (auto& [name, evb] : *variables) {
      if (evb.state == VariableInfo::Evaluated) {
        continue;
      }
      assert(evb.state != VariableInfo::Evaluating);
      ctxt->setProperty(name, _evalRet(evb.variable.get()));
      evb.state = VariableInfo::Evaluated;
    }
  }
}

String SyntaxEvaluatorImpl::generateBackTrace(const String& msg) const {
  OStringStream oss;
  oss << "Msg: " << msg << "\n"
      << "EVStack trace: \n"
      << stackDump();
  return oss.str();
}

String SyntaxEvaluatorImpl::dump(const Evaluables& l) {
  OStringStream oss;
  oss << "( ";
  for (auto& e : l) {
    oss << syntaxOf(*e);
  }
  oss << " )";
  return oss.str();
}

String SyntaxEvaluatorImpl::syntaxOf(const Evaluable& e) {
  return SyntaxValidator{}.generateSyntax(e);
}

template <class _optype_t, _optype_t _optype_val,
          template <class> class _std_op,
          template <class, class> class _applier_impl>
EvaluatedValue SyntaxEvaluatorImpl::applyOp(
    const EvaluatedOnReadValues& frame) {
  EvaluatedValue firstEvaluated = frame.front();
  return firstEvaluated->visitValue(
      [&frame, this](auto&& val) {
        using ValType =
            std::remove_const_t<std::remove_reference_t<decltype(val)>>;
        if constexpr (_type_traits::operationSupported<ValType>(_optype_val)) {
          return _applier_impl<ValType, _std_op<ValType>>::apply(frame);
        } else {
          evalThrow<EvaluationError>("operation ", _optype_val,
                                     " must apply on all arguments of ",
                                     typeid(val).name(), " only");
        }
        return EvaluatedValue{};
      },
      true);
}

template <class _Operation, class _Callable>
EvaluatedValue SyntaxEvaluatorImpl::evaluateOperator(const _Operation& op,
                                                     _Callable&& eval_func) {
  evaluateVariables(op.stackVariables);
  // evaluate all params
  EvaluatedOnReadValues inlevals;
  for (auto& p : op.params) {
    inlevals.emplace_back(this, *p);
  }
  // then apply the operation:
  return eval_func(inlevals);
}

template <class _compare_method, size_t expected_count, class _operation>
void SyntaxEvaluatorImpl::validateParamCount(const _operation& o) {
  evalThrowIf<SyntaxError>(_compare_method{}(o.params.size(), expected_count),
                           "Error: Invalid param count of `", o.type,
                           "` Expected: ", expected_count,
                           " - Real: ", o.params.size());
}

template <class _Operation>
void SyntaxEvaluatorImpl::makeSureUnaryOp(const _Operation& o) {
  validateParamCount<std::not_equal_to<>, 1>(o);
}

template <class T>
void SyntaxEvaluatorImpl::makeSureBinaryOp(
    const _OperatorBase<T, typename T::OperatorType>& o) {
  validateParamCount<std::less<>, 2>(o);
}

template <class T>
void SyntaxEvaluatorImpl::makeSureSingleBinaryOp(
    const _OperatorBase<T, typename T::OperatorType>& o) {
  validateParamCount<std::not_equal_to<>, 2>(o);
}

template <class T, T _optype, template <class> class _std_op>
EvaluatedValue SyntaxEvaluatorImpl::applySingleBinOp(
    const EvaluatedOnReadValues& ievals) {
  return applyOp<T, _optype, _std_op, ApplySingleBinOpImpl>(ievals);
}

template <class T, T _optype, template <class> class _std_op>
EvaluatedValue SyntaxEvaluatorImpl::applyMultiBinOp(
    const EvaluatedOnReadValues& ievals) {
  return applyOp<T, _optype, _std_op, ApplyMutiBinOpImpl>(ievals);
}

template <class T, T _optype, template <class> class _std_op>
EvaluatedValue SyntaxEvaluatorImpl::applyUnaryOp(
    const EvaluatedOnReadValues& ievals) {
  return applyOp<T, _optype, _std_op, ApplyUnaryOpImpl>(ievals);
}

EvaluatedValue SyntaxEvaluatorImpl::applyLogicalAndOp(
    const EvaluatedOnReadValues& ievals) {
  return applyOp<LogicalOperatorType, LogicalOperatorType::logical_and,
                 std::logical_and, ApplyLogicalAndOpImpl>(ievals);
}

EvaluatedValue SyntaxEvaluatorImpl::applyLogicalOrOp(
    const EvaluatedOnReadValues& ievals) {
  return applyOp<LogicalOperatorType, LogicalOperatorType::logical_or,
                 std::logical_or, ApplyLogicalOrOpImpl>(ievals);
}

SyntaxEvaluator::SyntaxEvaluator() : impl_{new SyntaxEvaluatorImpl} {}

SyntaxEvaluator::~SyntaxEvaluator() { delete impl_; }

EvaluatedValue SyntaxEvaluator::evaluate(const Evaluable& e,
                                         EvalContextPtr rootContext) {
  return impl_->evaluate(e, move(rootContext));
}

EvaluatedValue SyntaxEvaluator::evaluate(const EvaluablePtr& e,
                                         EvalContextPtr rootContext) {
  return impl_->evaluate(e, move(rootContext));
}

void SyntaxEvaluator::setDebugInfoCallback(
    DebugOutputCallback debugOutputCallback) {
  impl_->setDebugInfoCallback(move(debugOutputCallback));
}

}  // namespace jas
