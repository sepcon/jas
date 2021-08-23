#pragma once

#include <functional>

#include "jas/EvalContextIF.h"
#include "jas/EvaluableClasses.h"

namespace jas {

using DebugOutputCallback = std::function<void(const String&)>;
using EvaluatedValue = std::shared_ptr<JsonAdapter>;

EvaluatedValue makeEvaluatedVal(JsonAdapter value);

struct Evaluable;
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

    operator EvaluatedValue() const {
      if (!ed_) {
        ed_ = delegate_->_evalRet(&e_);
      }
      return ed_;
    }

    SyntaxEvaluatorImpl* delegate_;
    const Evaluable& e_;
    mutable EvaluatedValue ed_;
  };

  EvaluationStack stack_;
  DebugOutputCallback dbCallback_;
  int evalCount_ = 0;
  //-----------------------------------------------
  void eval(const DirectVal& v) override;
  void eval(const EvaluableMap& v) override;
  void eval(const EvaluableArray& v) override;
  void eval(const ArithmaticalOperator& op) override;
  void eval(const ArthmSelfAssignOperator& op) override;
  void eval(const LogicalOperator& op) override;
  void eval(const ComparisonOperator& op) override;
  void eval(const ListOperation& op) override;
  void eval(const FunctionInvocation& func) override;
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
  inline void stackDebugReturnVal(const EvaluatedValue& e,
                                  const Evaluable* evb);
  EvaluatedValue& stackReturnedVal();
  EvaluatedValue stackTakeReturnedVal();
  EvaluationFrame& stackTopFrame();
  const EvalContextPtr& stackTopContext();
  const EvalContextPtr& stackRootContext();
  EvaluatedValue evaluateSingleVar(const EvalContextPtr& ctxt,
                                   const String& varname,
                                   const VariableInfo& vi);
  void evaluateVariables(const StackVariablesPtr& params);
  String generateBackTrace(const String& msg) const;

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
            template <class, class, class> class _ApplierImpl, class _Params>
  EvaluatedValue applyOp(const _Params& ievals);
  template <class _OpType, _OpType _opType, template <class> class _std_op,
            class _Params>
  EvaluatedValue applySingleBinOp(const _Params& ievals);
  template <class _OpType, _OpType _opType, template <class> class _std_op,
            class _Params>
  EvaluatedValue applyMultiBinOp(const _Params& ievals);

  template <class _OpType, _OpType _opType, template <class> class _std_op,
            class _Params>
  EvaluatedValue applyUnaryOp(const _Params& ievals);
  template <class _Params>
  EvaluatedValue applyLogicalAndOp(const _Params& ievals);
  template <class _Params>
  EvaluatedValue applyLogicalOrOp(const _Params& ievals);
  static String syntaxOf(const Evaluable& e);

  template <class T, class _std_op, class _Params>
  struct ApplyUnaryOpImpl {
    static EvaluatedValue apply(const _Params& evals) {
      auto evaled = EvaluatedValue{evals.back()};
      throwIf<EvaluationError>(!evaled, "Evaluated to null");
      return makeEvaluatedVal(
          _std_op{}(EvaluatedValue { evals.back() }->get<T>()));
    }
  };

  template <class T, class _std_op, class _Params>
  struct ApplySingleBinOpImpl {
    static EvaluatedValue apply(const _Params& evals) {
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

  template <class T, class _std_op, class _Params>
  struct ApplyMutiBinOpImpl {
    static EvaluatedValue apply(const _Params& evals) {
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

  template <class T, class _std_op, bool untilVal, class _Params>
  struct ApplyLogicalOpImpl {
    static EvaluatedValue apply(const _Params& evals) {
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
  template <class T, class _std_op, class _Params>
  using ApplyLogicalAndOpImpl = ApplyLogicalOpImpl<T, _std_op, false, _Params>;
  template <class T, class _std_op, class _Params>
  using ApplyLogicalOrOpImpl = ApplyLogicalOpImpl<T, _std_op, true, _Params>;
};

}  // namespace jas
