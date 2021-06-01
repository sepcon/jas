#include "SyntaxEvaluator.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stack>
#include <string>

#include "EvaluableClasses.h"
#include "Exception.h"
#include "Keywords.h"
#include "String.h"
#include "SyntaxValidator.h"

namespace jas {

using EvaluatedValues = std::vector<EvaluatedValue>;

class SyntaxEvaluatorImpl : public EvaluatorBase {
 public:
  EvaluatedValue evaluated_result() const;
  EvaluatedValue evaluate(const Evaluable& e,
                          EvalContextPtr root_context = nullptr);
  EvaluatedValue evaluate(const EvaluablePtr& e,
                          EvalContextPtr root_context = nullptr);

 private:
  struct EvaluationFrame;
  struct InlineEvaluatedValue;
  using EvaluationStack = std::list<EvaluationFrame>;
  using InlineEvaluatedValues = std::vector<InlineEvaluatedValue>;

  struct EvaluationFrame {
    EvalContextPtr context;
    EvaluatedValues evaluatedVals;
    String debugInfo;
  };

  struct InlineEvaluatedValue {
    InlineEvaluatedValue(SyntaxEvaluatorImpl* delegate, const Evaluable& e)
        : delegate_(delegate), e_(e) {}

    operator EvaluatedValue() const {
      e_.accept(delegate_);
      auto evaluated = std::move(delegate_->stackTopLastElem());
      delegate_->stackTopFrame().evaluatedVals.pop_back();
      return evaluated;
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
  void eval(const ContextVal& fc) override;
  void eval(const Property& rv) override;

  /// Exceptions
  template <class _Exception, typename... _Msg>
  void evalThrowIf(bool cond, _Msg&&... msg);
  template <class _Exception, typename... _Msg>
  void evalThrow(_Msg&&...);

  /// Stack functions
  String stackDump() const;
  void stackPush(String debug_info = "__new_stack_frame",
                 EvalContextPtr ctxt = {});
  void stackTopAddVal(EvaluatedValue&& val);
  void stackTopAddVal(EvaluatedValue&& val, const Evaluable& ev);
  EvaluatedValue& stackTopLastElem();
  EvaluationFrame stackPop();
  EvaluationFrame& stackTopFrame();
  const EvalContextPtr& stackTopContext();
  String stackTraceMsg(const String& msg) const;
  EvaluatedValue evaluateOne(const EvaluablePtr& e, EvalContextPtr ctxt = {});
  String dump(const Evaluables& l);

  /// Evaluations:
  template <class _Operation, class _Callable>
  EvaluatedValue evaluateOperation(const _Operation& op, _Callable&& eval_func);
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
  EvaluatedValue applyOp(const InlineEvaluatedValues& ievals);
  template <class _OpType, _OpType _opType, template <class> class _std_op>
  EvaluatedValue applySingleBinOp(const InlineEvaluatedValues& ievals);
  template <class _OpType, _OpType _opType, template <class> class _std_op>
  EvaluatedValue applyMultiBinOp(const InlineEvaluatedValues& ievals);

  template <class _OpType, _OpType _opType, template <class> class _std_op>
  EvaluatedValue applyUnaryOp(const InlineEvaluatedValues& ievals);

  EvaluatedValue applyLogicalAndOp(const InlineEvaluatedValues& ievals);
  EvaluatedValue applyLogicalOrOp(const InlineEvaluatedValues& ievals);

  static String syntaxOf(const Evaluable& e);

  template <class T, class _std_op>
  struct ApplyUnaryOpImpl {
    static EvaluatedValue apply(const InlineEvaluatedValues& evals) {
      return EvaluatedValue{_std_op{}(EvaluatedValue{evals.back()}.get<T>())};
    }
  };

  template <class T, class _std_op>
  struct ApplySingleBinOpImpl {
    static EvaluatedValue apply(const InlineEvaluatedValues& evals) {
      return EvaluatedValue{_std_op{}(EvaluatedValue{evals.front()}.get<T>(),
                                      EvaluatedValue{evals.back()}.get<T>())};
    }
  };

  template <class T, class _std_op>
  struct ApplyMutiBinOpImpl {
    static EvaluatedValue apply(const InlineEvaluatedValues& evals) {
      auto it = std::begin(evals);
      auto v = EvaluatedValue{*it}.get<T>();
      _std_op applier;
      while (++it != std::end(evals)) {
        v = applier(v, EvaluatedValue{*it}.get<T>());
      }
      return EvaluatedValue{v};
    }
  };

  template <class T, class _std_op, bool untilVal>
  struct ApplyLogicalOpImpl {
    static EvaluatedValue apply(const InlineEvaluatedValues& evals) {
      bool v = !untilVal;
      _std_op applier;
      for (auto& e : evals) {
        v = applier(v, static_cast<bool>(EvaluatedValue{e}.get<T>()));
        if (v == untilVal) {
          break;
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
                                    __SUPPORT_ALL> {};

template <class T, class _OpType>
constexpr bool operationSupported(_OpType op) {
  return OperatorsSupported<T>::supported(op);
}
}  // namespace _details

#define __MC_BASIC_OPERATION_EVAL_START(op) try
#define __MC_BASIC_OPERATION_EVAL_END(op)                                \
  catch (const std::bad_variant_access&) {                               \
    evalThrow<EvaluationError>("Error while evaluating operation: ",     \
                               SyntaxValidator{}.generate_syntax(op),    \
                               " -> parameters of operation `", op.type, \
                               "` must be same type and NOT null");      \
  }

/// Exceptions
template <class _Exception, typename... _Msg>
void SyntaxEvaluatorImpl::evalThrowIf(bool cond, _Msg&&... msg) {
  if (cond) {
    throw_<_Exception>(stackTraceMsg(strJoin(msg...)));
  }
}

template <class _Exception, typename... _Msg>
void SyntaxEvaluatorImpl::evalThrow(_Msg&&... msg) {
  throw_<_Exception>(stackTraceMsg(strJoin(msg...)));
}

inline EvaluatedValue SyntaxEvaluatorImpl::evaluated_result() const {
  return stack_.back().evaluatedVals.back();
}

inline EvaluatedValue SyntaxEvaluatorImpl::evaluate(
    const Evaluable& e, EvalContextPtr root_context) {
  SyntaxValidator validator;
  EvaluatedValue evaluated;
  if (validator.validate(e)) {
    if (!stack_.empty()) {
      stack_.clear();
    }

    stackPush("__main_frame__", std::move(root_context));
    e.accept(this);
    evaluated = evaluated_result();
    stackPop();

  } else {
    throw_<SyntaxError>(validator.get_report());
  }

  return evaluated;
}

inline EvaluatedValue SyntaxEvaluatorImpl::evaluate(
    const EvaluablePtr& e, EvalContextPtr root_context) {
  if (e) {
    return evaluate(*e, std::move(root_context));
  } else {
    return {};
  }
}

inline void SyntaxEvaluatorImpl::eval(const DirectVal& v) {
  stackTopAddVal(v.value);
}

inline void SyntaxEvaluatorImpl::eval(const EvaluableMap& v) {
  JsonTrait::Object evaluated;
  for (auto& [key, val] : v.value) {
    try {
      val->accept(this);
      evaluated.emplace(key, std::move(stackTopLastElem().value));
      stackTopFrame().evaluatedVals.pop_back();
    } catch (const EvaluationError&) {
      evaluated.emplace(key, Json{});
    }
  }
  stackTopAddVal({std::move(evaluated)});
}

inline void SyntaxEvaluatorImpl::eval(const EvaluableArray& v) {
  JsonTrait::Array evaluated;
  for (auto& val : v.value) {
    try {
      val->accept(this);
      evaluated.push_back(std::move(stackTopLastElem().value));
      stackTopFrame().evaluatedVals.pop_back();
    } catch (const EvaluationError&) {
      evaluated.emplace_back(Json{});
    }
  }
  stackTopAddVal(std::move(evaluated));
}

inline void SyntaxEvaluatorImpl::eval(const ArithmaticalOperator& op) {
  __MC_BASIC_OPERATION_EVAL_START(op) {
    if ((op.type == ArithmeticOperatorType::bit_not) ||
        (op.type == ArithmeticOperatorType::negate)) {
      makeSureUnaryOp(op);

    } else {
      makeSureBinaryOp(op);
    }

    auto e = evaluateOperation(op, [&op, this](auto&& evaluatedVals) {
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
    });

    stackTopAddVal(std::move(e), op);
  }
  __MC_BASIC_OPERATION_EVAL_END(op)
}

inline void SyntaxEvaluatorImpl::eval(const LogicalOperator& op) {
  __MC_BASIC_OPERATION_EVAL_START(op) {
    if (op.type == LogicalOperatorType::logical_not) {
      makeSureUnaryOp(op);
    } else {
      makeSureBinaryOp(op);
    }

    auto e = evaluateOperation(op, [&op, this](auto&& evaluatedVals) {
      switch (op.type) {
        case LogicalOperatorType::logical_and:
          return applyLogicalAndOp(evaluatedVals);
        case LogicalOperatorType::logical_or:
          return applyLogicalOrOp(evaluatedVals);
        case LogicalOperatorType::logical_not:
          return applyUnaryOp<LogicalOperatorType,
                              LogicalOperatorType::logical_not,
                              std::logical_not>(evaluatedVals);
        default:
          return EvaluatedValue{};
      }
    });
    stackTopAddVal(std::move(e), op);
  }
  __MC_BASIC_OPERATION_EVAL_END(op)
}

inline void SyntaxEvaluatorImpl::eval(const ComparisonOperator& op) {
  __MC_BASIC_OPERATION_EVAL_START(op) {
    makeSureSingleBinaryOp(op);
    auto e = evaluateOperation(op, [&op, this](auto&& evaluated) {
      switch (op.type) {
        case ComparisonOperatorType::eq:
          return applySingleBinOp<ComparisonOperatorType,
                                  ComparisonOperatorType::eq, std::equal_to>(
              evaluated);
        case ComparisonOperatorType::gt:
          return applySingleBinOp<ComparisonOperatorType,
                                  ComparisonOperatorType::gt, std::greater>(
              evaluated);
        case ComparisonOperatorType::ge:
          return applySingleBinOp<ComparisonOperatorType,
                                  ComparisonOperatorType::ge,
                                  std::greater_equal>(evaluated);
        case ComparisonOperatorType::lt:
          return applySingleBinOp<ComparisonOperatorType,
                                  ComparisonOperatorType::lt, std::less>(
              evaluated);
        case ComparisonOperatorType::le:
          return applySingleBinOp<ComparisonOperatorType,
                                  ComparisonOperatorType::le, std::less_equal>(
              evaluated);
        case ComparisonOperatorType::neq:
          return applySingleBinOp<ComparisonOperatorType,
                                  ComparisonOperatorType::neq,
                                  std::not_equal_to>(evaluated);
        default:
          return EvaluatedValue{};
      }
    });
    stackTopAddVal(std::move(e), op);
  }
  __MC_BASIC_OPERATION_EVAL_END(op)
}

inline void SyntaxEvaluatorImpl::eval(const ListOperation& op) {
  auto op_name = syntaxOf(op);
  stackPush(op_name);
  EvaluatedValue list;
  if (op.list) {
    list = evaluateOne(op.list);
  }

  auto ctxt_list = stackTopContext()->subContexts(list, op.itemID);
  auto eval_impl = [&](const EvalContextPtr& ctxt) {
    auto evaluated = evaluateOne(op.cond, ctxt);
    evalThrowIf<EvaluationError>(
        !evaluated.isType<bool>(), "Invalid param type > operation: ", op_name,
        "` > expected: `boolean` > real_val: `", syntaxOf(evaluated), "`");
    return evaluated.get<bool>();
  };

  EvaluatedValue final_eval;
  switch (op.type) {
    case ListOperationType::any_of:
      final_eval =
          std::any_of(std::begin(ctxt_list), std::end(ctxt_list), eval_impl);
      break;
    case ListOperationType::all_of:
      final_eval =
          std::all_of(std::begin(ctxt_list), std::end(ctxt_list), eval_impl);
      break;
    case ListOperationType::none_of:
      final_eval =
          std::none_of(std::begin(ctxt_list), std::end(ctxt_list), eval_impl);
      break;
    case ListOperationType::size_of:
      final_eval = ctxt_list.size();
      break;
    case ListOperationType::count_if:
      final_eval =
          std::count_if(std::begin(ctxt_list), std::end(ctxt_list), eval_impl);
      break;
    default:
      break;
  }
  stackPop();
  stackTopAddVal(std::move(final_eval), op);
}

inline void SyntaxEvaluatorImpl::eval(const Function& func) {
  evalThrowIf<SyntaxError>(func.name.empty(),
                           "Function name must not be empty");
  stackTopAddVal(
      stackTopContext()->invokeFunction(func.name, evaluateOne(func.param)),
      func);
}

inline void SyntaxEvaluatorImpl::eval(const ContextVal& fc) {
  auto path = evaluateOne(fc.path);
  evalThrowIf<EvaluationError>(!path.isType<String>());
  stackTopAddVal(stackTopContext()->snapshotValue(path, fc.snapshot), fc);
}

void SyntaxEvaluatorImpl::eval(const Property& prop) {
  stackTopAddVal(stackTopContext()->getProperty(prop.id), prop);
}

inline String SyntaxEvaluatorImpl::stackDump() const {
  OStringStream oss;
  oss << std::boolalpha;
  int i = stack_.size();
  for (auto iframe = std::rbegin(stack_); iframe != std::rend(stack_);
       ++iframe) {
    oss << "#" << i-- << ": " << std::setw(15) << iframe->debugInfo;

    auto ival = std::begin(iframe->evaluatedVals);
    auto ival_end = std::end(iframe->evaluatedVals);
    if (ival != ival_end) {
      oss << " ==>> ( ";
      oss << syntaxOf(*ival);
      while (++ival != ival_end) {
        oss << ", " << syntaxOf(*ival);
      }
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

inline void SyntaxEvaluatorImpl::stackPush(String debug_info,
                                           EvalContextPtr ctxt) {
  if (!ctxt) {
    ctxt = stackTopContext();
  }
  stack_.emplace_back(EvaluationFrame{std::move(ctxt), EvaluatedValues{},
                                      std::move(debug_info)});
}

void SyntaxEvaluatorImpl::stackTopAddVal(EvaluatedValue&& val) {
  stackTopFrame().evaluatedVals.push_back(std::move(val));
}

inline void SyntaxEvaluatorImpl::stackTopAddVal(EvaluatedValue&& val,
                                                const Evaluable& ev) {
  if (!ev.id.empty()) {
    val.id = ev.id;
    stackTopContext()->setProperty(strJoin(prefix::property, ev.id), val);
  }
  stackTopAddVal(std::move(val));
}

inline EvaluatedValue& SyntaxEvaluatorImpl::stackTopLastElem() {
  return stackTopFrame().evaluatedVals.back();
}

inline SyntaxEvaluatorImpl::EvaluationFrame SyntaxEvaluatorImpl::stackPop() {
  auto top = std::move(stack_.back());
  stack_.pop_back();
  return top;
}

inline SyntaxEvaluatorImpl::EvaluationFrame&
SyntaxEvaluatorImpl::stackTopFrame() {
  return stack_.back();
}

inline const EvalContextPtr& SyntaxEvaluatorImpl::stackTopContext() {
  return stack_.back().context;
}

inline String SyntaxEvaluatorImpl::stackTraceMsg(const String& msg) const {
  OStringStream oss;
  oss << "Msg: " << msg << "\n"
      << "EVStack trace: \n"
      << stackDump();
  return oss.str();
}

inline EvaluatedValue SyntaxEvaluatorImpl::evaluateOne(const EvaluablePtr& e,
                                                       EvalContextPtr ctxt) {
  if (e) {
    stackPush(syntaxOf(*e), std::move(ctxt));
    e->accept(this);
    auto frame = stackPop();
    if (!frame.evaluatedVals.empty()) {
      return std::move(frame.evaluatedVals.back());
    }
  }
  return {};
}

inline String SyntaxEvaluatorImpl::dump(const Evaluables& l) {
  OStringStream oss;
  oss << "( ";
  for (auto& e : l) {
    oss << syntaxOf(*e);
  }
  oss << " )";
  return oss.str();
}

inline String SyntaxEvaluatorImpl::syntaxOf(const Evaluable& e) {
  return SyntaxValidator{}.generate_syntax(e);
}

template <class _optype_t, _optype_t _optype_val,
          template <class> class _std_op,
          template <class, class> class _applier_impl>
EvaluatedValue SyntaxEvaluatorImpl::applyOp(
    const InlineEvaluatedValues& frame) {
  EvaluatedValue firstEvaluated = frame.front();
  return firstEvaluated.visitValue([&frame, this](auto&& val) {
    using ValType = std::remove_const_t<std::remove_reference_t<decltype(val)>>;
    if constexpr (_details::operationSupported<ValType>(_optype_val)) {
      return _applier_impl<ValType, _std_op<ValType>>::apply(frame);
    } else {
      evalThrow<EvaluationError>("operation ", _optype_val,
                                 " must apply on all arguments of ",
                                 typeid(val).name(), " only");
    }
    return EvaluatedValue{};
  });
}

template <class _Operation, class _Callable>
EvaluatedValue SyntaxEvaluatorImpl::evaluateOperation(const _Operation& op,
                                                      _Callable&& eval_func) {
  stackPush(syntaxOf(op));
  // evaluate all params
  InlineEvaluatedValues inlevals;
  for (auto& p : op.params) {
    inlevals.emplace_back(this, *p);
  }
  // then apply the operation:
  auto e = eval_func(inlevals);
  stackPop();
  return e;
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
    const InlineEvaluatedValues& ievals) {
  return applyOp<T, _optype, _std_op, ApplySingleBinOpImpl>(ievals);
}

template <class T, T _optype, template <class> class _std_op>
EvaluatedValue SyntaxEvaluatorImpl::applyMultiBinOp(
    const InlineEvaluatedValues& ievals) {
  return applyOp<T, _optype, _std_op, ApplyMutiBinOpImpl>(ievals);
}

template <class T, T _optype, template <class> class _std_op>
EvaluatedValue SyntaxEvaluatorImpl::applyUnaryOp(
    const InlineEvaluatedValues& ievals) {
  return applyOp<T, _optype, _std_op, ApplyUnaryOpImpl>(ievals);
}

EvaluatedValue SyntaxEvaluatorImpl::applyLogicalAndOp(
    const InlineEvaluatedValues& ievals) {
  return applyOp<LogicalOperatorType, LogicalOperatorType::logical_and,
                 std::logical_and, ApplyLogicalAndOpImpl>(ievals);
}

EvaluatedValue SyntaxEvaluatorImpl::applyLogicalOrOp(
    const InlineEvaluatedValues& ievals) {
  return applyOp<LogicalOperatorType, LogicalOperatorType::logical_or,
                 std::logical_or, ApplyLogicalOrOpImpl>(ievals);
}

SyntaxEvaluator::SyntaxEvaluator() : impl_{new SyntaxEvaluatorImpl} {}

SyntaxEvaluator::~SyntaxEvaluator() { delete impl_; }

EvaluatedValue SyntaxEvaluator::evaluatedResult() const {
  return impl_->evaluated_result();
}

EvaluatedValue SyntaxEvaluator::evaluate(const Evaluable& e,
                                         EvalContextPtr rootContext) {
  return impl_->evaluate(e, std::move(rootContext));
}

EvaluatedValue SyntaxEvaluator::evaluate(const EvaluablePtr& e,
                                         EvalContextPtr rootContext) {
  return impl_->evaluate(e, std::move(rootContext));
}

}  // namespace jas
