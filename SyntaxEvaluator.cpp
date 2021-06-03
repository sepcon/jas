#include "SyntaxEvaluator.h"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <list>
#include <sstream>
#include <string>

#include "EvaluableClasses.h"
#include "Exception.h"
#include "Keywords.h"
#include "String.h"
#include "SyntaxValidator.h"

#define lambda_on_this(method, ...) [&] { return method(__VA_ARGS__); }

namespace jas {
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
  return filtered;
}

template <class _Callable>
EvaluatedValue transform(const JsonArray& list, _Callable&& transf) {
  JsonArray transformed;
  for (auto& item : list) {
    JsonTrait::add(transformed, transf(item));
  }
  return transformed;
}

class SyntaxEvaluatorImpl : public EvaluatorBase {
 public:
  EvaluatedValue evaluatedResult() const;
  EvaluatedValue evaluate(const Evaluable& e,
                          EvalContextPtr rootContext = nullptr);
  EvaluatedValue evaluate(const EvaluablePtr& e,
                          EvalContextPtr rootContext = nullptr);
  ~SyntaxEvaluatorImpl();

 private:
  struct EvaluationFrame;
  struct EvaluatedOnReadValue;
  using EvaluationStack = std::list<EvaluationFrame>;
  using EvaluatedOnReadValues = std::vector<EvaluatedOnReadValue>;

  struct EvaluationFrame {
    EvalContextPtr context;
    EvaluatedValue returnedValue;
    String debugInfo;
  };

  struct EvaluatedOnReadValue {
    EvaluatedOnReadValue(SyntaxEvaluatorImpl* delegate, const Evaluable& e)
        : delegate_(delegate), e_(e) {}

    operator EvaluatedValue() const {
      e_.accept(delegate_);
      return delegate_->takeStackReturnedVal();
    }

    SyntaxEvaluatorImpl* delegate_;
    const Evaluable& e_;
  };

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

  /// Exceptions
  template <class _Exception, typename... _Msg>
  void evalThrowIf(bool cond, _Msg&&... msg);
  template <class _Exception, typename... _Msg>
  void evalThrow(_Msg&&...);

  /// Stack functions
  String stackDump() const;
  void stackPush(String ctxtID, String debugInfo = JASSTR("__new_stack_frame"),
                 JsonAdapter contextData = {});
  void stackReturn(EvaluatedValue&& val);
  void stackReturn(EvaluatedValue&& val, const Evaluable& ev);
  EvaluatedValue& stackReturnedVal();
  EvaluatedValue takeStackReturnedVal();
  EvaluationFrame stackPop();
  EvaluationFrame& stackTopFrame();
  const EvalContextPtr& stackTopContext();
  void evaluateContextParams(const ContextParams& params);
  String stackTraceMsg(const String& msg) const;
  EvaluatedValue evaluateOne(const EvaluablePtr& e, JsonAdapter ctxtData = {});
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
      return EvaluatedValue{_std_op{}(EvaluatedValue{evals.back()}.get<T>())};
    }
  };

  template <class T, class _std_op>
  struct ApplySingleBinOpImpl {
    static EvaluatedValue apply(const EvaluatedOnReadValues& evals) {
      EvaluatedValue lhsEv = evals.front();
      EvaluatedValue rhsEv = evals.back();
      try {
        return EvaluatedValue{_std_op{}(lhsEv.get<T>(), rhsEv.get<T>())};
      } catch (const DirectVal::TypeError&) {
        throw_<DirectVal::TypeError>("[", JsonTrait::dump(lhsEv), ", ",
                                     JsonTrait::dump(rhsEv), "]");
        return {};
      }
    }
  };

  template <class T, class _std_op>
  struct ApplyMutiBinOpImpl {
    static EvaluatedValue apply(const EvaluatedOnReadValues& evals) {
      auto it = std::begin(evals);
      EvaluatedValue lastEv = *it++;
      auto v = lastEv.get<T>();
      _std_op applier;
      while (it != std::end(evals)) {
        EvaluatedValue nextEv = *it;
        try {
          v = applier(v, nextEv.get<T>());
          std::swap(lastEv, nextEv);
        } catch (const DirectVal::TypeError&) {
          throw_<DirectVal::TypeError>("[", JsonTrait::dump(lastEv.value), ", ",
                                       JsonTrait::dump(nextEv.value), "]");
        }
        ++it;
      }
      return EvaluatedValue{v};
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
          v = applier(v, static_cast<bool>(nextEv.get<T>()));
          if (v == untilVal) {
            break;
          }
        } catch (const DirectVal::TypeError&) {
          throw_<DirectVal::TypeError>("[", v, ", ", JsonTrait::dump(nextEv),
                                       "]");
        }
      }
      return EvaluatedValue{v};
    }
  };
  template <class T, class _std_op>
  using ApplyLogicalAndOpImpl = ApplyLogicalOpImpl<T, _std_op, false>;
  template <class T, class _std_op>
  using ApplyLogicalOrOpImpl = ApplyLogicalOpImpl<T, _std_op, true>;

  EvaluationStack stack_;
};

namespace _details {
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
}  // namespace _details

#define __MC_STACK_START(ctxtID, debugInfo, ...)                  \
  {                                                               \
    stackPush(ctxtID, debugInfo, __VA_ARGS__);                    \
    auto __remember_pop = callWhenExit(lambda_on_this(stackPop)); \
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
  }                                                 \
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
    throw_<StackUnwin<_Exception>>(stackTraceMsg(strJoin(msg...)));
  }
}

template <class _Exception, typename... _Msg>
void SyntaxEvaluatorImpl::evalThrow(_Msg&&... msg) {
  throw_<StackUnwin<_Exception>>(stackTraceMsg(strJoin(msg...)));
}

EvaluatedValue SyntaxEvaluatorImpl::evaluatedResult() const {
  return stack_.back().returnedValue;
}

EvaluatedValue SyntaxEvaluatorImpl::evaluate(const Evaluable& e,
                                             EvalContextPtr rootContext) {
  SyntaxValidator validator;
  EvaluatedValue evaluated;
  if (validator.validate(e)) {
    if (!stack_.empty()) {
      stack_.clear();
    }

    stack_.emplace_back(EvaluationFrame{move(rootContext), EvaluatedValue{},
                                        JASSTR("__main_frame__")});
    auto rememberPop = callWhenExit(lambda_on_this(stackPop));
    e.accept(this);
    evaluated = evaluatedResult();

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

SyntaxEvaluatorImpl::~SyntaxEvaluatorImpl() {
  while (!stack_.empty()) {
    stack_.pop_back();
  }
}

void SyntaxEvaluatorImpl::eval(const DirectVal& v) { stackReturn(v.value); }

void SyntaxEvaluatorImpl::eval(const EvaluableMap& v) {
  JsonObject evaluated;
  if (!v.id.empty()) {
    JsonTrait::add(evaluated, String{keyword::id}, v.id);
  }

  evaluateContextParams(v.ctxtParams);

  for (auto& [key, val] : v.value) {
    //    try {
    val->accept(this);
    evaluated.emplace(key, takeStackReturnedVal());
    //    } catch (const EvaluationError&) {
    //      evaluated.emplace(key, Json{});
    //    }
  }
  stackReturn({move(evaluated)}, v);
}

void SyntaxEvaluatorImpl::eval(const EvaluableArray& v) {
  JsonArray evaluated;
  for (auto& val : v.value) {
    try {
      val->accept(this);
      evaluated.emplace_back(takeStackReturnedVal());
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
      case ArithmeticOperatorType::divides:
        return applyMultiBinOp<ArithmeticOperatorType,
                               ArithmeticOperatorType::divides, std::divides>(
            evaluatedVals);
      case ArithmeticOperatorType::minus:
        return applyMultiBinOp<ArithmeticOperatorType,
                               ArithmeticOperatorType::minus, std::minus>(
            evaluatedVals);
      case ArithmeticOperatorType::modulus:
        return applyMultiBinOp<ArithmeticOperatorType,
                               ArithmeticOperatorType::modulus, std::modulus>(
            evaluatedVals);
      case ArithmeticOperatorType::multiplies:
        return applyMultiBinOp<ArithmeticOperatorType,
                               ArithmeticOperatorType::multiplies,
                               std::multiplies>(evaluatedVals);
      case ArithmeticOperatorType::negate:
        return applyUnaryOp<ArithmeticOperatorType,
                            ArithmeticOperatorType::negate, std::negate>(
            evaluatedVals);
      case ArithmeticOperatorType::plus:
        return applyMultiBinOp<ArithmeticOperatorType,
                               ArithmeticOperatorType::plus, std::plus>(
            evaluatedVals);
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
    auto apply = [&](auto&& stdop) {
      return EvaluatedValue{stdop(EvaluatedValue{evaluated.front()},
                                  EvaluatedValue{evaluated.back()})};
    };

    switch (op.type) {
      case ComparisonOperatorType::eq:
        return apply(std::equal_to<Json>());
      case ComparisonOperatorType::gt:
        return applySingleBinOp<ComparisonOperatorType,
                                ComparisonOperatorType::gt, std::greater>(
            evaluated);
      case ComparisonOperatorType::ge:
        return applySingleBinOp<ComparisonOperatorType,
                                ComparisonOperatorType::ge, std::greater_equal>(
            evaluated);
      case ComparisonOperatorType::lt:
        return applySingleBinOp<ComparisonOperatorType,
                                ComparisonOperatorType::lt, std::less>(
            evaluated);
      case ComparisonOperatorType::le:
        return applySingleBinOp<ComparisonOperatorType,
                                ComparisonOperatorType::le, std::less_equal>(
            evaluated);
      case ComparisonOperatorType::neq:
        return apply(std::not_equal_to<Json>());
      default:
        return EvaluatedValue{};
    }
    __MC_BASIC_OPERATION_EVAL_END_RETURN(op, EvaluatedValue{})
  });
  stackReturn(move(e), op);
}

void SyntaxEvaluatorImpl::eval(const ListOperation& op) {
  auto opSyntax = syntaxOf(op);
  EvaluatedValue finalEvaled;
  __MC_STACK_START(strJoin(op.type), opSyntax);
  EvaluatedValue vlist;
  if (op.list) {
    vlist = evaluateOne(op.list);
  }

  evalThrowIf<EvaluationError>(
      !vlist.isType<JsonArray>(),
      "`@list` input of list operation was not evaluated to array type");

  evaluateContextParams(op.ctxtParams);
  auto list = vlist.get<JsonArray>();
  int itemIdx = 0;
  auto eval_impl = [this, &itemIdx, &op, &opSyntax](const Json& data) {
    __MC_STACK_START(strJoin(itemIdx++), syntaxOf(*op.cond), data);
    auto evaluated = evaluateOne(op.cond, data);
    evalThrowIf<EvaluationError>(
        !evaluated.isType<bool>(), "Invalid param type > operation: ", opSyntax,
        "` > expected: `boolean` > real_val: `", evaluated.dump(), "`");
    return evaluated.get<bool>();
    __MC_STACK_END_RETURN(false);
  };

  switch (op.type) {
    case ListOperationType::any_of:
      finalEvaled = std::any_of(std::begin(list), std::end(list), eval_impl);
      break;
    case ListOperationType::all_of:
      finalEvaled = std::all_of(std::begin(list), std::end(list), eval_impl);
      break;
    case ListOperationType::none_of:
      finalEvaled = std::none_of(std::begin(list), std::end(list), eval_impl);
      break;
    case ListOperationType::count_if:
      finalEvaled = std::count_if(std::begin(list), std::end(list), eval_impl);
      break;
    case ListOperationType::filter_if:
      finalEvaled = filter_if(list, eval_impl);
      break;
    case ListOperationType::transform:
      finalEvaled = transform(list, [this, &itemIdx, &op](const Json& data) {
        __MC_STACK_START(strJoin(itemIdx++), syntaxOf(*op.cond), data);
        return evaluateOne(op.cond, data);
        __MC_STACK_END_RETURN(EvaluatedValue{})
      });
      break;
    default:
      break;
  }
  __MC_STACK_END
  stackReturn(move(finalEvaled), op);
}

void SyntaxEvaluatorImpl::eval(const Function& func) {
  evalThrowIf<SyntaxError>(func.name.empty(),
                           "Function name must not be empty");
  EvaluatedValue evaluated;
  __MC_STACK_START(func.name, syntaxOf(func));
  evaluateContextParams(func.ctxtParams);
  evaluated = stackTopContext()->invoke(func.name, evaluateOne(func.param));
  __MC_STACK_END
  stackReturn(move(evaluated), func);
}

void SyntaxEvaluatorImpl::eval(const VariableFieldQuery& query) {
  auto& varname = query.id;
  auto var = stackTopContext()->property(varname);
  do {
    if (JsonTrait::isNull(var)) {
      stackReturn({});
      break;
    }

    JsonTrait::Path path;
    for (auto& evbField : query.field_path) {
      auto field = evaluateOne(evbField);
      evalThrowIf<EvaluationError>(!JsonTrait::isString(field),
                                   "Cannot evaluated to a valid path");
      path /= JsonTrait::get<String>(field);
    }

    stackReturn(JsonTrait::get(var.value, path));
  } while (false);
}

void SyntaxEvaluatorImpl::eval(const Variable& prop) {
  // don't pass prop to second arg of stackReturn because it will store the
  // property again
  EvaluatedValue val = stackTopContext()->property(prop.id);
  stackReturn(move(val));
}

String SyntaxEvaluatorImpl::stackDump() const {
  OStringStream oss;
  oss << std::boolalpha;
  int i = static_cast<int>(stack_.size());
  for (auto iframe = std::rbegin(stack_); iframe != std::rend(stack_);
       ++iframe) {
    oss << "#" << i-- << ": " << std::setw(15) << iframe->debugInfo;

    if (!iframe->returnedValue.isNull()) {
      oss << " ==>> ( ";
      oss << iframe->returnedValue.dump();
    } else {
      oss << " ==>> ( evaluating...";
    }

    oss << " )";
    if (auto ctxinfo = iframe->context->debugInfo(); !ctxinfo.empty()) {
      oss << " --> [ctxt]: " << iframe->context->debugInfo();
    }
    oss << "\n";
  }
  return oss.str();
}

void SyntaxEvaluatorImpl::stackPush(String ctxtID, String debugInfo,
                                    JsonAdapter contextData) {
  stack_.emplace_back(
      EvaluationFrame{stackTopContext()->subContext(ctxtID, move(contextData)),
                      EvaluatedValue{}, move(debugInfo)});
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

EvaluatedValue jas::SyntaxEvaluatorImpl::takeStackReturnedVal() {
  using namespace std;
  EvaluatedValue e;
  swap(e, stackReturnedVal());
  return e;
}

SyntaxEvaluatorImpl::EvaluationFrame SyntaxEvaluatorImpl::stackPop() {
  auto top = move(stack_.back());
  stack_.pop_back();
  return top;
}

SyntaxEvaluatorImpl::EvaluationFrame& SyntaxEvaluatorImpl::stackTopFrame() {
  return stack_.back();
}

const EvalContextPtr& SyntaxEvaluatorImpl::stackTopContext() {
  return stack_.back().context;
}

void SyntaxEvaluatorImpl::evaluateContextParams(const ContextParams& params) {
  auto ctxt = stackTopContext();
  for (auto& [name, evb] : params) {
    ctxt->setProperty(name, evaluateOne(evb));
  }
}

String SyntaxEvaluatorImpl::stackTraceMsg(const String& msg) const {
  OStringStream oss;
  oss << "Msg: " << msg << "\n"
      << "EVStack trace: \n"
      << stackDump();
  return oss.str();
}

EvaluatedValue SyntaxEvaluatorImpl::evaluateOne(const EvaluablePtr& evb,
                                                JsonAdapter ctxtData) {
  EvaluatedValue evaluated;
  if (evb) {
    evb->accept(this);
    auto& frame = stackTopFrame();
    if (!JsonTrait::isNull(frame.returnedValue)) {
      // dont move, swap with json null is safer
      std::swap(evaluated, frame.returnedValue);
    }
  }
  return evaluated;
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
  return firstEvaluated.visitValue(
      [&frame, this](auto&& val) {
        using ValType =
            std::remove_const_t<std::remove_reference_t<decltype(val)>>;
        if constexpr (_details::operationSupported<ValType>(_optype_val)) {
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
  __MC_STACK_START(strJoin(op.type), syntaxOf(op));
  evaluateContextParams(op.ctxtParams);
  // evaluate all params
  EvaluatedOnReadValues inlevals;
  for (auto& p : op.params) {
    inlevals.emplace_back(this, *p);
  }
  // then apply the operation:
  return eval_func(inlevals);
  __MC_STACK_END_RETURN(EvaluatedValue{});
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

EvaluatedValue SyntaxEvaluator::evaluatedResult() const {
  return impl_->evaluatedResult();
}

EvaluatedValue SyntaxEvaluator::evaluate(const Evaluable& e,
                                         EvalContextPtr rootContext) {
  return impl_->evaluate(e, move(rootContext));
}

EvaluatedValue SyntaxEvaluator::evaluate(const EvaluablePtr& e,
                                         EvalContextPtr rootContext) {
  return impl_->evaluate(e, move(rootContext));
}

}  // namespace jas
