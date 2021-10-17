#pragma once

#include <functional>

#include "EvalContextIF.h"
#include "EvaluableClasses.h"
#include "Var.h"

namespace jas {

using DebugOutputCallback = std::function<void(const String&)>;
class ModuleManager;
class Evaluable;
class EvaluationStack;
class EvaluationFrame;
using EvaluationFramePtr = std::shared_ptr<EvaluationFrame>;

class SyntaxEvaluatorImpl : public EvaluatorIF {
 public:
  Var evaluate(const Evaluable& e, EvalContextPtr rootContext = nullptr);
  Var evaluate(const EvaluablePtr& e, EvalContextPtr rootContext = nullptr);

  void setDebugInfoCallback(DebugOutputCallback);

  SyntaxEvaluatorImpl();
  ~SyntaxEvaluatorImpl();

  EvaluationStack* stack_ = nullptr;
  DebugOutputCallback dbCallback_;
  int evalCount_ = 0;
  //-----------------------------------------------
  void eval(const Constant& v) override;
  void eval(const EvaluableDict& v) override;
  void eval(const EvaluableList& v) override;
  void eval(const ArithmaticalOperator& op) override;
  void eval(const ArthmSelfAssignOperator& op) override;
  void eval(const LogicalOperator& op) override;
  void eval(const ComparisonOperator& op) override;
  void eval(const ListAlgorithm& op) override;
  void eval(const ContextFI& func) override;
  void eval(const EvaluatorFI& func) override;
  void eval(const ModuleFI& func) override;
  void eval(const MacroFI& macro) override;
  void eval(const ObjectPropertyQuery& query) override;
  void eval(const Variable& rv) override;
  void eval(const ContextArgument& arg) override;
  void eval(const ContextArgumentsInfo& arginf) override;

  Var* _queryOrEvalVariable(const String& variableName);
  void _evalOnStack(const Evaluable* e, String ctxtID = {},
                    ContextArguments ctxtInput = {});
  Var evalAndReturn(const Evaluable* e, String ctxtID = {},
                    ContextArguments ctxtData = {});
  template <class _Exception, typename... _Msg>
  void stackUnwindThrow(_Msg&&...);

  inline void debugStackReturnedValue(const Var& e, const Evaluable* evb);
  Var& stackReturnedVal();
  Var stackTakeReturnedVal();
  Var* evaluateSingleVar(const EvaluationFramePtr& frame, const String& varname,
                         const VariableEvalInfo& vi);
  void evaluateLocalSymbols(const UseStackEvaluable& evb);
  String generateBackTrace(const String& msg) const;

  /// Evaluations:
  template <class _Operator, class _Callable>
  Var evaluateOperator(const _Operator& op, _Callable&& eval_func);
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
  Var applyOp(const _Params& ievals);
  template <class _OpType, _OpType _opType, template <class> class _std_op,
            class _Params>
  Var applySingleBinOp(const _Params& ievals);
  template <class _OpType, _OpType _opType, template <class> class _std_op,
            class _Params>
  Var applyMultiBinOp(const _Params& ievals);

  template <class _OpType, _OpType _opType, template <class> class _std_op,
            class _Params>
  Var applyUnaryOp(const _Params& ievals);
  template <class _Params>
  Var applyLogicalAndOp(const _Params& ievals);
  template <class _Params>
  Var applyLogicalOrOp(const _Params& ievals);
  static String syntaxOf(const Evaluable& e);

  template <class T, class _std_op, class _Params>
  struct ApplyUnaryOpImpl {
    static Var apply(const _Params& evals) {
      auto evaled = Var{evals.back()};
      __jas_throw_if(EvaluationError, evaled.isNull(), "Evaluated to null");
      return (_std_op{}(Var{evals.back()}.getValue<T>()));
    }
  };

  template <class T, class _std_op, class _Params>
  struct ApplySingleBinOpImpl {
    static Var apply(const _Params& evals) {
      Var lhsEv = evals.front();
      Var rhsEv = evals.back();
      try {
        return (_std_op{}(lhsEv.getValue<T>(), rhsEv.getValue<T>()));
      } catch (const TypeError&) {
        __jas_throw(TypeError, "(", lhsEv.dump(), ", ", rhsEv.dump(), ")");
        return {};
      }
    }
  };

  template <class T, class _std_op, class _Params>
  struct ApplyMutiBinOpImpl {
    static Var apply(const _Params& evals) {
      auto it = std::begin(evals);
      Var lastEv = *it++;
      auto v = lastEv.getValue<T>();
      _std_op applier;
      while (it != std::end(evals)) {
        Var nextEv = *it;
        __jas_throw_if(EvaluationError, nextEv.isNull(), "operate on null");
        try {
          v = applier(v, nextEv.getValue<T>());
          std::swap(lastEv, nextEv);
        } catch (const TypeError&) {
          __jas_throw(TypeError, "(", lastEv.dump(), ", ", nextEv.dump(), ")");
        }
        ++it;
      }
      return v;
    }
  };

  template <class T, class _std_op, bool untilVal, class _Params>
  struct ApplyLogicalOpImpl {
    static Var apply(const _Params& evals) {
      bool v = !untilVal;
      _std_op applier;
      for (auto& e : evals) {
        Var nextEv = e;
        try {
          v = applier(v, nextEv.getValue<T>());
          if (v == untilVal) {
            break;
          }
        } catch (const TypeError&) {
          __jas_throw(TypeError, "(", v, ", ", nextEv.dump(), ")");
        }
      }
      return (std::move(v));
    }
  };
  template <class T, class _std_op, class _Params>
  using ApplyLogicalAndOpImpl = ApplyLogicalOpImpl<T, _std_op, false, _Params>;
  template <class T, class _std_op, class _Params>
  using ApplyLogicalOrOpImpl = ApplyLogicalOpImpl<T, _std_op, true, _Params>;
};

}  // namespace jas
