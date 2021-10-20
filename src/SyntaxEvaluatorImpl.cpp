#include "jas/SyntaxEvaluatorImpl.h"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <memory>
#include <numeric>
#include <sstream>

#include "details/EvaluationStack.h"
#include "jas/EvaluableClasses.h"
#include "jas/Exception.h"
#include "jas/Keywords.h"
#include "jas/ModuleManager.h"
#include "jas/OpTraits.h"
#include "jas/String.h"
#include "jas/SyntaxValidator.h"
#include "jas/TypesName.h"

#define lambda_on_this(method, ...) [&] { return method(__VA_ARGS__); }

namespace jas {

using std::make_shared;
using std::move;
using Vars = std::vector<Var>;
struct EvaluatedOnReadValue;
using EvaluatedOnReadValues = std::vector<EvaluatedOnReadValue>;

struct EvaluatedOnReadValue {
  EvaluatedOnReadValue(SyntaxEvaluatorImpl* delegate, const Evaluable& e)
      : delegate_(delegate), e_(e) {}

  operator Var() const {
    if (ed_.isNull()) {
      ed_ = delegate_->evalAndReturn(&e_);
    }
    return ed_;
  }

  SyntaxEvaluatorImpl* delegate_;
  const Evaluable& e_;
  mutable Var ed_;
};

template <class _Exception>
struct StackUnwin : public _Exception {
  using _Base = _Exception;
  using _Base::_Base;
};

template <class _Callable>
Var filter_if(const Vars& list, _Callable&& cond) {
  auto filtered = Var::list();
  for (auto& item : list) {
    if (cond(item)) {
      filtered.add(item);
    }
  }
  return filtered;
}

template <class _Callable>
Var transform(const Vars& list, _Callable&& transf) {
  auto transformed = Var::list();
  for (auto& item : list) {
    transformed.add(transf(item));
  }
  return transformed;
}

inline auto operator+(const Var::List& lhs, const Var::List& rhs) {
  auto out = lhs;
  out.insert(std::end(out), std::begin(rhs), std::end(rhs));
  return out;
}

inline auto operator+(const Var::Dict& lhs, const Var::Dict& rhs) {
  auto out = lhs;
  out.insert(std::begin(rhs), std::end(rhs));
  return out;
}

#define __MC_STACK_START(ctxtID, evb, ...)  \
  stack_->push(ctxtID, evb, ##__VA_ARGS__); \
  try {
#define __MC_STACK_END                                     \
  }                                                        \
  catch (const StackUnwin<SyntaxError>& e) {               \
    throw e;                                               \
  }                                                        \
  catch (const StackUnwin<TypeError>& e) {                 \
    throw e;                                               \
  }                                                        \
  catch (const StackUnwin<EvaluationError>& e) {           \
    throw e;                                               \
  }                                                        \
  catch (const StackUnwin<jas::Exception>& e) {            \
    throw e;                                               \
  }                                                        \
  catch (jas::Exception & e) {                             \
    stackUnwindThrow<jas::Exception>(e.details);           \
  }                                                        \
  catch (std::exception & e) {                             \
    stackUnwindThrow<jas::Exception>(e.what());            \
  }                                                        \
  catch (...) {                                            \
    stackUnwindThrow<jas::Exception>("Unknown exception"); \
  }

#define __MC_STACK_END_RETURN(ret) __MC_STACK_END return ret;

#define __MC_BASIC_OPERATION_EVAL_START(op) try {
#define __MC_BASIC_OPERATION_EVAL_END(op)                    \
  }                                                          \
  catch (const TypeError& e) {                               \
    stackUnwindThrow<EvaluationError>(                       \
        "Evaluation Error: ", SyntaxValidator::syntaxOf(op), \
        " -> parameters of operation `", op.type,            \
        "` must be same type and NOT null: \n", e.details);  \
  }
#define __MC_BASIC_OPERATION_EVAL_END_RETURN(op, defaultRet) \
  __MC_BASIC_OPERATION_EVAL_END(op) return defaultRet;

/// Exceptions

#define __stackUnwindThrowIf(_Exception, cond, ...) \
  if (cond) {                                       \
    stackUnwindThrow<_Exception>(__VA_ARGS__);      \
  }

#define __stackUnwindThrowVariableNotFoundIf(var, varname) \
  __stackUnwindThrowIf(EvaluationError, !var, "Property not found: ", varname);

template <class _Exception, typename... _Msg>
void SyntaxEvaluatorImpl::stackUnwindThrow(_Msg&&... msg) {
  throw_<StackUnwin<_Exception>>(generateBackTrace(strJoin(msg...)));
}

Var SyntaxEvaluatorImpl::evaluate(const Evaluable& e,
                                  EvalContextPtr rootContext) {
  SyntaxValidator validator;
  Var evaluated;
  if (validator.validate(e)) {
    // push root context to stack as main entry
    stack_->init(move(rootContext), &e);
    e.accept(this);
    evaluated = stackTakeReturnedVal();
  } else {
    __jas_throw(SyntaxError, validator.getReport());
  }
  return evaluated;
}

Var SyntaxEvaluatorImpl::evaluate(const EvaluablePtr& e,
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

SyntaxEvaluatorImpl::SyntaxEvaluatorImpl() : stack_(new EvaluationStack) {}

SyntaxEvaluatorImpl::~SyntaxEvaluatorImpl() { delete stack_; }

void SyntaxEvaluatorImpl::eval(const Constant& v) {
  stack_->return_(Var::ref(v.value));
}

void SyntaxEvaluatorImpl::eval(const EvaluableDict& v) {
  evaluateLocalSymbols(v);
  if (v.value.empty() && v.localVariables && v.localVariables->size() == 1) {
    auto& [varname, _] = *(v.localVariables->begin());
    auto val = stack_->top()->context->lookupVariable(varname);
    assert(val && "Variable must be available here");
    stack_->return_(*val);
  } else {
    auto evaluated = Var::dict();
    for (auto& [key, val] : v.value) {
      evaluated.add(key, evalAndReturn(val.get(), strJoin(key, '.')));
    }

    stack_->return_(evaluated.empty() ? Var{} : move(evaluated));
  }
}

void SyntaxEvaluatorImpl::eval(const EvaluableList& v) {
  auto evaluated = Var::list();
  auto itemIdx = 0;
  for (auto& val : v.value) {
    evaluated.add(evalAndReturn(val.get(), strJoin(itemIdx++, '.')));
  }
  stack_->return_(move(evaluated));
}

void SyntaxEvaluatorImpl::eval(const ArithmaticalOperator& op) {
  if ((op.type == aot::bit_not) || (op.type == aot::negate)) {
    makeSureUnaryOp(op);
  } else {
    makeSureBinaryOp(op);
  }

  evaluateLocalSymbols(op);
  auto e = evaluateOperator(op, [&op, this](auto&& evaluatedVals) {
    __MC_BASIC_OPERATION_EVAL_START(op)
    switch (op.type) {
      case aot::bit_and:
        return applyMultiBinOp<aot, aot::bit_and, std::bit_and>(evaluatedVals);
      case aot::bit_not:
        return applyUnaryOp<aot, aot::bit_not, std::bit_not>(evaluatedVals);
      case aot::bit_or:
        return applyMultiBinOp<aot, aot::bit_or, std::bit_or>(evaluatedVals);
      case aot::bit_xor:
        return applyMultiBinOp<aot, aot::bit_xor, std::bit_xor>(evaluatedVals);
      case aot::modulus:
        return applyMultiBinOp<aot, aot::modulus, std::modulus>(evaluatedVals);
      case aot::divides:
        return applyMultiBinOp<aot, aot::divides, std::divides>(evaluatedVals);
      case aot::minus:
        return applyMultiBinOp<aot, aot::minus, std::minus>(evaluatedVals);
      case aot::multiplies:
        return applyMultiBinOp<aot, aot::multiplies, std::multiplies>(
            evaluatedVals);
      case aot::negate:
        return applyUnaryOp<aot, aot::negate, std::negate>(evaluatedVals);
      case aot::plus:
        return applyMultiBinOp<aot, aot::plus, std::plus>(evaluatedVals);
      default:
        return Var{};
    }
    __MC_BASIC_OPERATION_EVAL_END_RETURN(op, Var{})
  });

  stack_->return_(move(e));
}

void SyntaxEvaluatorImpl::eval(const ArthmSelfAssignOperator& op) {
  assert(op.params.size() == 2);
  __MC_BASIC_OPERATION_EVAL_START(op)
  evaluateLocalSymbols(op);
  auto var = op.params.front();
  auto opStr = strJoin(op.type);
  __stackUnwindThrowIf(EvaluationError, !isType<Variable>(var.get()),
                       "first argument of operator `", opStr,
                       "` must be a variable");

  auto varVal = evalAndReturn(var.get(), opStr);
  __stackUnwindThrowIf(EvaluationError, varVal.isNull(), "Variable ",
                       static_cast<const Variable*>(var.get())->name,
                       " has not been initialized yet");

  auto paramVal = evalAndReturn(op.params.back().get(), opStr);
  __stackUnwindThrowIf(EvaluationError, paramVal.isNull(),
                       "Parameter to operator `", opStr, "` evaluated to null");

  varVal.detach();
  std::vector<Var> operands = {varVal, paramVal};
  Var result;
  switch (op.type) {
    case asot::s_plus:
      result = applySingleBinOp<asot, asot::s_plus, std::plus>(operands);
      break;
    case asot::s_minus:
      result = applySingleBinOp<asot, asot::s_minus, std::minus>(operands);
      break;
    case asot::s_multiplies:
      result =
          applySingleBinOp<asot, asot::s_multiplies, std::multiplies>(operands);
      break;
    case asot::s_divides:
      result = applySingleBinOp<asot, asot::s_divides, std::divides>(operands);
      break;
    case asot::s_modulus: {
      result = applySingleBinOp<asot, asot::s_modulus, std::modulus>(operands);
    } break;
    case asot::s_bit_and: {
      result = applySingleBinOp<asot, asot::s_bit_and, std::bit_and>(operands);
    } break;
    case asot::s_bit_or: {
      result = applySingleBinOp<asot, asot::s_bit_or, std::bit_or>(operands);
    } break;
    case asot::s_bit_xor: {
      result = applySingleBinOp<asot, asot::s_bit_xor, std::bit_xor>(operands);
    } break;
    default:
      __jas_throw(EvaluationError, "Unexpected operator");
      break;
  }

  varVal.assign(move(result));
  stack_->return_(move(varVal));
  __MC_BASIC_OPERATION_EVAL_END(op)
}

void SyntaxEvaluatorImpl::eval(const LogicalOperator& op) {
  if (op.type == lot::logical_not) {
    makeSureUnaryOp(op);
  } else {
    makeSureBinaryOp(op);
  }

  auto e = evaluateOperator(op, [&op, this](auto&& evaluatedVals) {
    __MC_BASIC_OPERATION_EVAL_START(op)
    switch (op.type) {
      case lot::logical_and:
        return applyLogicalAndOp(evaluatedVals);
      case lot::logical_or:
        return applyLogicalOrOp(evaluatedVals);
      case lot::logical_not:
        return applyUnaryOp<lot, lot::logical_not, std::logical_not>(
            evaluatedVals);
      default:
        return Var{};
    }
    __MC_BASIC_OPERATION_EVAL_END_RETURN(op, Var{})
  });
  stack_->return_(move(e));
}

void SyntaxEvaluatorImpl::eval(const ComparisonOperator& op) {
  makeSureSingleBinaryOp(op);
  auto e = evaluateOperator(op, [&op, this](auto&& evaluated) {
    __MC_BASIC_OPERATION_EVAL_START(op)
    auto __applyOp = [&](auto&& stdop) {
      Var first = evaluated.front();
      Var second = evaluated.back();
      return stdop(first, second);
    };

    switch (op.type) {
      case cot::eq:
        return __applyOp(std::equal_to{});
      case cot::gt:
        return __applyOp(std::greater{});
      case cot::ge:
        return __applyOp(std::greater_equal{});
      case cot::lt:
        return __applyOp(std::less{});
      case cot::le:
        return __applyOp(std::less_equal{});
      case cot::neq:
        return __applyOp(std::not_equal_to{});
      default:
        return false;
    }
    __MC_BASIC_OPERATION_EVAL_END_RETURN(op, false)
  });
  stack_->return_(move(e));
}

void SyntaxEvaluatorImpl::eval(const ListAlgorithm& op) {
  Var finalEvaled;
  Var vlist;

  evaluateLocalSymbols(op);

  if (op.list) {
    vlist = evalAndReturn(op.list.get(), strJoin(op.type));
  }

  __stackUnwindThrowIf(EvaluationError, !vlist.isList(),
                       "`@list` input of ListAlgorithm ", op.type,
                       " was not evaluated to array type");

  auto& list = vlist.asList();
  int itemIdx = 0;
  auto eval_impl = [this, &itemIdx, &op](const Var& data) {
    auto evaluated = evalAndReturn(op.cond.get(), strJoin(itemIdx++),
                                   ContextArguments{data});
    __stackUnwindThrowIf(EvaluationError, !evaluated.isBool(),
                         "Invalid param type > operation: ", syntaxOf(op),
                         "` > expected: `boolean` > real_val: `",
                         evaluated.dump(), "`");
    return evaluated.getValue<bool>();
  };

  switch (op.type) {
    case lsaot::any_of:
      finalEvaled = std::any_of(std::begin(list), std::end(list), eval_impl);
      break;
    case lsaot::all_of:
      finalEvaled = std::all_of(std::begin(list), std::end(list), eval_impl);
      break;
    case lsaot::none_of:
      finalEvaled = std::none_of(std::begin(list), std::end(list), eval_impl);
      break;
    case lsaot::count_if:
      finalEvaled = std::count_if(std::begin(list), std::end(list), eval_impl);
      break;
    case lsaot::filter_if:
      finalEvaled = filter_if(list, eval_impl);
      break;
    case lsaot::transform:
      finalEvaled = transform(list, [this, &itemIdx, &op](const Var& data) {
        return evalAndReturn(op.cond.get(), strJoin(itemIdx++),
                             ContextArguments{data});
      });
      break;
    default:
      break;
  }
  stack_->return_(move(finalEvaled));
}

template <class _FI>
Var _evalFIParam(SyntaxEvaluatorImpl* evaluator,
                 const FunctionInvocationBase<_FI>& fi) {
  assert(!fi.name.empty());
  evaluator->evaluateLocalSymbols(fi);
  return evaluator->evalAndReturn(fi.param.get());
}
void SyntaxEvaluatorImpl::eval(const ContextFI& fi) {
  stack_->return_(
      stack_->top()->context->invoke(fi.name, _evalFIParam(this, fi)));
}

void SyntaxEvaluatorImpl::eval(const EvaluatorFI& fi) {
  if (fi.name == StringView{keyword::return_ + 1}) {
    stack_->return_(_evalFIParam(this, fi));
  } else {
    assert(false);
  }
}

void SyntaxEvaluatorImpl::eval(const ModuleFI& fi) {
  assert(fi.module);
  //  auto evaluatedParam = _evalFIParam(this, fi);
  evaluateLocalSymbols(fi);
  auto funcRet = fi.module->eval(fi.name, fi.param, this);
  stack_->return_(move(funcRet));
}

void SyntaxEvaluatorImpl::eval(const MacroFI& macro) {
  evaluateLocalSymbols(macro);
  auto args = evalAndReturn(macro.param.get());
  assert((args.isNull() || args.isList()) &&
         "evaluated params must be null(aka void) or a list of arguments");
  stack_->top()->context->args(args.isNull() ? ContextArguments{}
                                             : args.asList());
  stack_->return_(evalAndReturn(macro.macro.get()));
}

void SyntaxEvaluatorImpl::eval(const ObjectPropertyQuery& query) {
  auto object = evalAndReturn(query.object.get());
  do {
    if (object.isNull()) {
      stack_->return_(Var{});
      break;
    }
    for (auto& evbField : query.propertyPath) {
      auto field = evalAndReturn(evbField.get());
      if (field.isString()) {
        object = object.getPath(field.asString());
      } else if (field.isInt()) {
        object = object.getAt(field.getValue<size_t>());
      } else {
        stackUnwindThrow<EvaluationError>("Cannot evaluated to a valid path: ",
                                          field.dump());
      }
      if (object.isNull()) {
        break;
      }
    }
    stack_->return_(move(object));
  } while (false);
}

void SyntaxEvaluatorImpl::eval(const Variable& variable) {
  auto val = stack_->top()->context->lookupVariable(variable.name);
  if (!val) {
    val = _findAndEvalNotInitializedVariableOrThrow(variable.name);
  }
  stack_->return_(*val);
}

void SyntaxEvaluatorImpl::eval(const ContextArgument& arg) {
  stack_->return_(stack_->top()->context->arg(arg.index));
}

void SyntaxEvaluatorImpl::eval(const ContextArgumentsInfo& arginf) {
  switch (arginf.type) {
    case ContextArgumentsInfo::Type::ArgCount:
      stack_->return_(stack_->top()->context->args().size());
      break;
    case ContextArgumentsInfo::Type::Args:
      stack_->return_(stack_->top()->context->args());
      break;
  }
}

Var* SyntaxEvaluatorImpl::_findAndEvalNotInitializedVariableOrThrow(
    const String& variableName) {
  Var* val = nullptr;
  // loop through stack to find the not evaluated property
  auto currentFrame = stack_->top();
  do {
    assert(currentFrame->evb->useStack());
    auto currentEvb = static_cast<const UseStackEvaluable*>(currentFrame->evb);
    if (!currentEvb->localVariables) {
      currentFrame = currentFrame->parent;
      continue;
    }

    if (auto itProp = currentEvb->localVariables->find(variableName);
        itProp != std::end(*currentEvb->localVariables)) {
      auto& varInfo = itProp->second;
      auto varStatus = currentFrame->variableStatus(variableName);
      // if variable was not found on top context, then it should not be
      // evaluated yet
      assert(varStatus != VariableStatus::Evaluated);
      __stackUnwindThrowIf(
          EvaluationError, varStatus == VariableStatus::Evaluating,
          "Cyclic reference detected on variable: $", variableName);
      val = evaluateSingleVar(currentFrame, variableName, varInfo);
      break;
    }
    currentFrame = currentFrame->parent;
  } while (currentFrame);
  __jas_throw_if(EvaluationError, !val, "Unknown reference to variable `$",
                 variableName, "`");
  return val;
}

void SyntaxEvaluatorImpl::_evalOnStack(const Evaluable* e, String ctxtID,
                                       ContextArguments ctxtInput) {
  assert(e);
  __MC_STACK_START(
      strJoin(move(ctxtID), static_cast<const UseStackEvaluable*>(e)->typeID()),
      e, std::move(ctxtInput));
  e->accept(this);

  __MC_STACK_END
}

Var SyntaxEvaluatorImpl::evalAndReturn(const Evaluable* e, String ctxtID,
                                       ContextArguments ctxtData) {
  Var evaluated;
  if (e) {
    if (e->useStack()) {
      _evalOnStack(e, move(ctxtID), move(ctxtData));
      evaluated = stackTakeReturnedVal();
    } else {
      e->accept(this);
      std::swap(evaluated, stackReturnedVal());
      if (!isType<Constant>(e)) {
        debugStackReturnedValue(evaluated, e);
      }
    }
  }
  return evaluated;
}

void SyntaxEvaluatorImpl::debugStackReturnedValue(const Var& e,
                                                  const Evaluable* evb) {
  if (dbCallback_ && evb) {
    auto addUseCount = [&](auto&& syntax) {
#ifndef __JAS_DEEP_DEBUG
      if (isType<Variable>(evb)) {
        return strJoin(syntax, std::hex, "  (", e.address(), " - ref:",
                       e.isRef() ? e.asRef()->useCount() : e.useCount(), ")");
      } else {
        return strJoin(syntax, std::hex, "  (", e.address(), ")");
      }
#else
      return std::move(syntax);
#endif
    };
    auto syntax = addUseCount(syntaxOf(*evb));

    if (syntax.size() > 60) {
      dbCallback_(strJoin(std::setw(3), ++evalCount_, JASSTR(". "), syntax,
                          "\n", std::setw(60), JASSTR("\t\t`--> "), ": ",
                          e.dump()));
    } else {
      dbCallback_(strJoin(std::setw(3), ++evalCount_, JASSTR(". "), std::left,
                          std::setw(60), syntax, JASSTR(": "), e.dump()));
    }
  }
}

Var& SyntaxEvaluatorImpl::stackReturnedVal() {
  return stack_->top()->returnedValue;
}

Var SyntaxEvaluatorImpl::stackTakeReturnedVal() {
  Var e = move(stackReturnedVal());
  debugStackReturnedValue(e, stack_->top()->evb);
  stack_->pop();
  return e;
}

Var* SyntaxEvaluatorImpl::evaluateSingleVar(const EvaluationFramePtr& frame,
                                            const String& varname,
                                            const VariableEvalInfo& vi) {
  Var* ret = nullptr;
  auto savedFrame = stack_->top();
  stack_->top(frame);
  frame->startEvaluatingVar(varname);
  auto val = evalAndReturn(vi.value.get(), varname);
  if (vi.type == VariableEvalInfo::Declaration) {
    ret = frame->context->putVariable(varname, move(val));
  } else {
    ret = frame->context->lookupVariable(varname);
    if (!ret) {
      // Case 2: Variable assignment, it must be initialized in its parent scope
      // for this situation, the variable is queried before its initialization,
      // then we need to pop current frame to eval the variable, then re-push
      // the top frame after initialization complete
      auto savedFrame1 = stack_->top();
      stack_->pop();
      ret = _findAndEvalNotInitializedVariableOrThrow(varname);
      stack_->repush(savedFrame1);
    }
    ret->assign(move(val));
  }
  frame->finishEvaluatingVar(varname);
  stack_->top(move(savedFrame));
  return ret;
}

void SyntaxEvaluatorImpl::evaluateLocalSymbols(const UseStackEvaluable& evb) {
  if (evb.localVariables) {
    auto currentFrame = stack_->top();
    if (!currentFrame->variableStatusMapPtr) {
      currentFrame->variableStatusMapPtr = make_shared<VariableStatusMap>();
    }
    for (auto& [varname, var] : *(evb.localVariables)) {
      auto varStatus = currentFrame->variableStatus(varname);
      assert(varStatus != VariableStatus::Evaluating);
      if (varStatus != VariableStatus::Evaluated) {
        evaluateSingleVar(currentFrame, varname, var);
      }
    }
  }
}

String SyntaxEvaluatorImpl::generateBackTrace(const String& msg) const {
  return strJoin("Msg: ", msg, "\n", "EVStack trace: \n", stack_->dump(),
                 "#__main__\n");
}

String SyntaxEvaluatorImpl::syntaxOf(const Evaluable& e) {
  return SyntaxValidator::syntaxOf(e);
}

template <class _optype_t, _optype_t _optype_val,
          template <class> class _std_op,
          template <class, class, class> class _applier_impl, class _Params>
Var SyntaxEvaluatorImpl::applyOp(const _Params& frame) {
  Var firstEvaluated = frame.front();
  return firstEvaluated.visitValue(
      [&](auto&& val) {
        using ValType = std::decay_t<decltype(val)>;
        auto _throwNotApplicableErr = [&] {
          throw_<TypeError>("operator ", _optype_val,
                            " is not applicable on type: ", typeNameOf(val),
                            " with value: ", firstEvaluated);
        };
        if constexpr (op_traits::operationSupported<ValType>(_optype_val)) {
          return _applier_impl<ValType, _std_op<ValType>, _Params>::apply(
              frame);
        } else {
          _throwNotApplicableErr();
          return Var{};
        }
      },
      true);
}

template <class _Operation, class _Callable>
Var SyntaxEvaluatorImpl::evaluateOperator(const _Operation& op,
                                          _Callable&& eval_func) {
  evaluateLocalSymbols(op);
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
  __stackUnwindThrowIf(
      SyntaxError, _compare_method{}(o.params.size(), expected_count),
      "Error: Invalid param count of `", o.type, "` Expected: ", expected_count,
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

template <class T, T _optype, template <class> class _std_op, class _Params>
Var SyntaxEvaluatorImpl::applySingleBinOp(const _Params& ievals) {
  return applyOp<T, _optype, _std_op, ApplySingleBinOpImpl>(ievals);
}

template <class T, T _optype, template <class> class _std_op, class _Params>
Var SyntaxEvaluatorImpl::applyMultiBinOp(const _Params& ievals) {
  return applyOp<T, _optype, _std_op, ApplyMutiBinOpImpl>(ievals);
}

template <class T, T _optype, template <class> class _std_op, class _Params>
Var SyntaxEvaluatorImpl::applyUnaryOp(const _Params& ievals) {
  return applyOp<T, _optype, _std_op, ApplyUnaryOpImpl>(ievals);
}
template <class _Params>
Var SyntaxEvaluatorImpl::applyLogicalAndOp(const _Params& ievals) {
  return applyOp<lot, lot::logical_and, std::logical_and,
                 ApplyLogicalAndOpImpl>(ievals);
}
template <class _Params>
Var SyntaxEvaluatorImpl::applyLogicalOrOp(const _Params& ievals) {
  return applyOp<lot, lot::logical_or, std::logical_or, ApplyLogicalOrOpImpl>(
      ievals);
}
}  // namespace jas
