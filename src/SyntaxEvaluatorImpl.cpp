#include "jas/SyntaxEvaluatorImpl.h"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <memory>
#include <numeric>
#include <sstream>

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

#define __MC_STACK_START(ctxtID, evb, ...) \
  stackPush(ctxtID, evb, ##__VA_ARGS__);   \
  try {
#define __MC_STACK_END                                     \
  }                                                        \
  catch (const StackUnwin<SyntaxError>& e) {               \
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
#define __MC_BASIC_OPERATION_EVAL_END(op)                           \
  }                                                                 \
  catch (const TypeError& e) {                                      \
    stackUnwindThrow<EvaluationError>(                              \
        "Evaluation Error: ", SyntaxValidator{}.generateSyntax(op), \
        " -> parameters of operation `", op.type,                   \
        "` must be same type and NOT null: ", e.details);           \
  }
#define __MC_BASIC_OPERATION_EVAL_END_RETURN(op, defaultRet) \
  __MC_BASIC_OPERATION_EVAL_END(op) return defaultRet;

/// Exceptions

#define __stackUnwindThrowIf(_Exception, cond, ...) \
  if (cond) {                                       \
    stackUnwindThrow<_Exception>(__VA_ARGS__);      \
  }

template <class _Exception, typename... _Msg>
void SyntaxEvaluatorImpl::stackUnwindThrow(_Msg&&... msg) {
  throw_<StackUnwin<_Exception>>(generateBackTrace(strJoin(msg...)));
}

Var SyntaxEvaluatorImpl::evaluate(const Evaluable& e,
                                  EvalContextPtr rootContext) {
  SyntaxValidator validator;
  Var evaluated;
  if (validator.validate(e)) {
    if (!stack_.empty()) {
      stack_.clear();
    }
    // push root context to stack as main entry
    stack_.emplace_back(move(rootContext), Var{}, &e);
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

SyntaxEvaluatorImpl::SyntaxEvaluatorImpl(ModuleManager* moduleMgr)
    : moduleMgr_(moduleMgr) {}

SyntaxEvaluatorImpl::~SyntaxEvaluatorImpl() {
  while (!stack_.empty()) {
    stack_.pop_back();
  }
}

void SyntaxEvaluatorImpl::eval(const Constant& v) {
  stackReturn(Var::ref(v.value));
}

void SyntaxEvaluatorImpl::eval(const EvaluableDict& v) {
  auto evaluated = Var::dict();
  if (!v.id.empty()) {
    evaluated.add(String{keyword::id}, v.id);
  }

  evaluateVariables(v.stackVariables);

  for (auto& [key, val] : v.value) {
    evaluated.add(key, _evalRet(val.get(), key));
  }
  stackReturn(move(evaluated), v);
}

void SyntaxEvaluatorImpl::eval(const EvaluableList& v) {
  auto evaluated = Var::list();
  auto itemIdx = 0;
  for (auto& val : v.value) {
    evaluated.add(_evalRet(val.get(), strJoin(itemIdx)));
  }
  stackReturn(move(evaluated));
}

void SyntaxEvaluatorImpl::eval(const ArithmaticalOperator& op) {
  if ((op.type == aot::bit_not) || (op.type == aot::negate)) {
    makeSureUnaryOp(op);
  } else {
    makeSureBinaryOp(op);
  }

  evaluateVariables(op.stackVariables);
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

  stackReturn(move(e), op);
}

void SyntaxEvaluatorImpl::eval(const ArthmSelfAssignOperator& op) {
  assert(op.params.size() == 2);
  __MC_BASIC_OPERATION_EVAL_START(op)
  evaluateVariables(op.stackVariables);
  auto var = op.params.front();
  auto varVal = _evalRet(var.get());
  __stackUnwindThrowIf(EvaluationError, varVal.isNull(), "Variable ", var->id,
                       " has not initialized yet");
  auto paramVal = _evalRet(op.params.back().get());
  __stackUnwindThrowIf(EvaluationError, paramVal.isNull(),
                       "Parameter to operator `", op.type,
                       "` evaluated to null");

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
  stackReturn(move(varVal), op);
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
  stackReturn(move(e), op);
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
  stackReturn(move(e), op);
}

void SyntaxEvaluatorImpl::eval(const ListAlgorithm& op) {
  Var finalEvaled;
  Var vlist;

  evaluateVariables(op.stackVariables);

  if (op.list) {
    vlist = _evalRet(op.list.get());
  }

  __stackUnwindThrowIf(
      EvaluationError, !vlist.isList(),
      "`@list` input of list operation was not evaluated to array type");

  auto& list = vlist.asList();
  int itemIdx = 0;
  auto eval_impl = [this, &itemIdx, &op](const Var& data) {
    auto evaluated = _evalRet(op.cond.get(), strJoin(itemIdx++), data);
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
        return _evalRet(op.cond.get(), strJoin(itemIdx++), data);
      });
      break;
    default:
      break;
  }
  stackReturn(move(finalEvaled), op);
}

template <class _FI>
Var _evalFIParam(SyntaxEvaluatorImpl* evaluator,
                 const FunctionInvocationBase<_FI>& fi) {
  assert(!fi.name.empty());
  evaluator->evaluateVariables(fi.stackVariables);
  return evaluator->_evalRet(fi.param.get());
}
void SyntaxEvaluatorImpl::eval(const ContextFI& fi) {
  stackReturn(stackTopContext()->invoke(fi.name, _evalFIParam(this, fi)));
}

void SyntaxEvaluatorImpl::eval(const EvaluatorFI& fi) {
  if (fi.name == StringView{keyword::return_ + 1}) {
    stackReturn(_evalFIParam(this, fi));
  } else {
    assert(false);
  }
}

void SyntaxEvaluatorImpl::eval(const ModuleFI& fi) {
  assert(fi.module);
  auto evaluatedParam = _evalFIParam(this, fi);
  auto funcRet = fi.module->eval(fi.name, evaluatedParam, this);
  stackReturn(move(funcRet), fi);
}

void SyntaxEvaluatorImpl::eval(const VariableFieldQuery& query) {
  auto& varname = query.id;
  auto var = _queryOrEvalVariable(varname);
  do {
    if (var->isNull()) {
      stackReturn(Var{});
      break;
    }

    auto dest = *var;
    for (auto& evbField : query.field_path) {
      auto field = _evalRet(evbField.get());
      if (field.isString()) {
        dest = dest.getAt(field.asString());
      } else if (field.isInt()) {
        dest = dest.getAt(field.getValue<size_t>());
      } else {
        stackUnwindThrow<EvaluationError>("Cannot evaluated to a valid path: ",
                                          field.dump());
      }
      if (dest.isNull()) {
        break;
      }
    }
    stackReturn(move(dest));
  } while (false);
}

void SyntaxEvaluatorImpl::eval(const Variable& prop) {
  stackReturn(*_queryOrEvalVariable(prop.id));
}

Var* SyntaxEvaluatorImpl::_queryOrEvalVariable(const String& variableName) {
  Var* val = nullptr;
  try {
    val = stackTopContext()->variable(variableName);
  } catch (const EvaluationError&) {
    // loop through stack to find the not evaluated property
    for (auto it = std::rbegin(stack_); it != std::rend(stack_); ++it) {
      if (!it->variables) {
        continue;
      }
      if (auto itProp = it->variables->find(variableName);
          itProp != std::end(*it->variables)) {
        auto& varInfo = *itProp;
        __stackUnwindThrowIf(
            EvaluationError, varInfo.state == VariableInfo::Evaluating,
            "Cyclic reference detected on variable: $", variableName);
        val = evaluateSingleVar(it->context, variableName, varInfo);
        break;
      }
    }
    __jas_throw_if(EvaluationError, !val, "Unknown reference to variable `$",
                   variableName, "`");
  }
  return val;
}

void SyntaxEvaluatorImpl::_evalOnStack(const Evaluable* e, String ctxtID,
                                       Var ctxtData) {
  assert(e);
  __MC_STACK_START(move(ctxtID), e, std::move(ctxtData));
  e->accept(this);
  __MC_STACK_END
}

Var SyntaxEvaluatorImpl::_evalRet(const Evaluable* e, String ctxtID,
                                  Var ctxtData) {
  Var evaluated;
  if (e) {
    if (e->useStack()) {
      _evalOnStack(e, move(ctxtID), move(ctxtData));
      evaluated = stackTakeReturnedVal();
    } else {
      e->accept(this);
      std::swap(evaluated, stackReturnedVal());
      if (!isType<Constant>(e)) {
        stackDebugReturnVal(evaluated, e);
      }
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
                                    Var contextData) {
  stack_.push_back(EvaluationFrame{
      stackTopContext()->subContext(ctxtID, move(contextData)), Var{}, evb});
}

void SyntaxEvaluatorImpl::stackReturn(Var val) {
  stackTopFrame().returnedValue = move(val);
}

void SyntaxEvaluatorImpl::stackReturn(Var val, const Evaluable& ev) {
  if (!ev.id.empty()) {
    stackTopContext()->setVariable(ev.id, val);
  }
  stackReturn(move(val));
}

void SyntaxEvaluatorImpl::stackDebugReturnVal(const Var& e,
                                              const Evaluable* evb) {
  if (dbCallback_ && evb) {
    auto addUseCount = [&](auto&& syntax) {
#ifdef __JAS_DEEP_DEBUG
      if (isType<Variable>(evb)) {
        return strJoin(syntax, std::hex, "  (", e.address(),
                       " - ref:", e.useCount(), ")");
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
  return stackTopFrame().returnedValue;
}

Var SyntaxEvaluatorImpl::stackTakeReturnedVal() {
  Var e = move(stackReturnedVal());
  stackDebugReturnVal(e, stackTopFrame().evb);
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

const EvalContextPtr& SyntaxEvaluatorImpl::stackRootContext() {
  return stack_.front().context;
}

Var* SyntaxEvaluatorImpl::evaluateSingleVar(const EvalContextPtr& ctxt,
                                            const String& varname,
                                            const VariableInfo& vi) {
  vi.state = VariableInfo::Evaluating;
  auto val = _evalRet(vi.variable.get());
  vi.state = VariableInfo::Evaluated;
  if (vi.type == VariableInfo::Declaration) {
    return ctxt->setVariable(varname, move(val));
  } else {
    auto var = ctxt->variable(varname);
    var->assign(move(val));
    return var;
  }
}

void SyntaxEvaluatorImpl::evaluateVariables(
    const StackVariablesPtr& variables) {
  if (variables) {
    auto ctxt = stackTopContext();
    // some variables might refer to variable in same stack frame
    stackTopFrame().variables = variables;
    for (auto& var : *(stackTopFrame().variables)) {
      if (var.state == VariableInfo::Evaluated) {
        continue;
      }
      evaluateSingleVar(ctxt, var.name, var);
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

String SyntaxEvaluatorImpl::syntaxOf(const Evaluable& e) {
  return SyntaxValidator{}.generateSyntax(e);
}

template <class _optype_t, _optype_t _optype_val,
          template <class> class _std_op,
          template <class, class, class> class _applier_impl, class _Params>
Var SyntaxEvaluatorImpl::applyOp(const _Params& frame) {
  Var firstEvaluated = frame.front();
  return firstEvaluated.visitValue(
      [&frame, this](auto&& val) {
        using ValType = std::decay_t<decltype(val)>;
        auto _throwNotApplicableErr = [&] {
          stackUnwindThrow<TypeError>(
              "Operator ", _optype_val,
              " is not applicable on type: ", typeNameOf(val));
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
