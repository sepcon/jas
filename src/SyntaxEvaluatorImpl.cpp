#include "jas/SyntaxEvaluatorImpl.h"

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <memory>
#include <numeric>
#include <sstream>

#include "jas/CIF.h"
#include "jas/EvaluableClasses.h"
#include "jas/Exception.h"
#include "jas/Keywords.h"
#include "jas/OpTraits.h"
#include "jas/String.h"
#include "jas/SyntaxValidator.h"
#include "jas/jaop.h"

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

static Json toJson(const JAdapterPtr& jap) {
  if (jap) {
    return *jap;
  } else {
    return {};
  }
}

EvaluatedValue makeEvaluatedVal(JsonAdapter value) {
  return make_shared<JsonAdapter>(move(value));
}

template <class _Callable>
EvaluatedValue filter_if(const JsonArray& list, _Callable&& cond) {
  auto filtered = JsonTrait::array();
  for (auto& item : list) {
    if (cond(item)) {
      JsonTrait::add(filtered, item);
    }
  }
  return makeEvaluatedVal(move(filtered));
}

template <class _Callable>
EvaluatedValue transform(const JsonArray& list, _Callable&& transf) {
  auto transformed = JsonTrait::array();
  for (auto& item : list) {
    JsonTrait::add(transformed, toJson(transf(item)));
  }
  return makeEvaluatedVal(move(transformed));
}

void setContextProperty(const EvalContextPtr& ctxt, const String& propName,
                        const EvaluatedValue& val) {
  if (val.use_count() == 1) {
    ctxt->setProperty(propName, val);
  } else {
    ctxt->setProperty(propName, makeEvaluatedVal(*val));
  }
}

template <class _StdOperator, class _Params>
static JsonAdapter binaryOperationAccumulate(const _Params& evaluatedVals) {
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
    stack_.emplace_back(move(rootContext), EvaluatedValue{}, &e);
    e.accept(this);
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
  auto evaluated = JsonTrait::object();
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
  auto evaluated = JsonTrait::array();
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
        return makeEvaluatedVal(
            binaryOperationAccumulate<std::divides<>>(evaluatedVals));
      case aot::minus:
        return makeEvaluatedVal(
            binaryOperationAccumulate<std::minus<>>(evaluatedVals));
      case aot::multiplies:
        return makeEvaluatedVal(
            binaryOperationAccumulate<std::multiplies<>>(evaluatedVals));
      case aot::negate:
        return applyUnaryOp<aot, aot::negate, std::negate>(evaluatedVals);
      case aot::plus:
        return makeEvaluatedVal(
            binaryOperationAccumulate<std::plus<>>(evaluatedVals));
      default:
        return EvaluatedValue{};
    }
    __MC_BASIC_OPERATION_EVAL_END_RETURN(op, EvaluatedValue{})
  });

  stackReturn(move(e), op);
}

void SyntaxEvaluatorImpl::eval(const ArthmSelfAssignOperator& op) {
  assert(op.params.size() == 2);
  __MC_BASIC_OPERATION_EVAL_START(op)
  evaluateVariables(op.stackVariables);
  auto var = op.params.front();
  auto val = _evalRet(var.get());
  evalThrowIf<EvaluationError>(!val, "Variable ", var->id,
                               " has not initialized yet");
  auto paramVal = _evalRet(op.params.back().get());
  evalThrowIf<EvaluationError>(!paramVal, "Parameter to operator `", op.type,
                               "` evaluated to null");

  switch (op.type) {
    case asot::s_plus:
      *val = (*val + *paramVal);
      break;
    case asot::s_minus:
      *val = (*val - *paramVal);
      break;
    case asot::s_multiplies:
      *val = (*val * *paramVal);
      break;
    case asot::s_divides:
      *val = (*val / *paramVal);
      break;
    case asot::s_modulus: {
      *val = *applySingleBinOp<asot, asot::s_modulus, std::modulus>(
          EvaluatedValues{val, paramVal});
    } break;
    case asot::s_bit_and: {
      *val = *applySingleBinOp<asot, asot::s_bit_and, std::bit_and>(
          EvaluatedValues{val, paramVal});
    } break;
    case asot::s_bit_or: {
      *val = *applySingleBinOp<asot, asot::s_bit_or, std::bit_or>(
          EvaluatedValues{val, paramVal});
    } break;
    case asot::s_bit_xor: {
      *val = *applySingleBinOp<asot, asot::s_bit_xor, std::bit_xor>(
          EvaluatedValues{val, paramVal});
    } break;
    default:
      throw_<EvaluationError>("Unexpected operator");
      break;
  }
  stackReturn(move(val), op);
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
      !vlist || !JsonTrait::isArray(vlist->value),
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
    case lsot::any_of:
      finalEvaled = makeEvaluatedVal(
          std::any_of(std::begin(list), std::end(list), eval_impl));
      break;
    case lsot::all_of:
      finalEvaled = makeEvaluatedVal(
          std::all_of(std::begin(list), std::end(list), eval_impl));
      break;
    case lsot::none_of:
      finalEvaled = makeEvaluatedVal(
          std::none_of(std::begin(list), std::end(list), eval_impl));
      break;
    case lsot::count_if:
      finalEvaled = makeEvaluatedVal(
          std::count_if(std::begin(list), std::end(list), eval_impl));
      break;
    case lsot::filter_if:
      finalEvaled = filter_if(list, eval_impl);
      break;
    case lsot::transform:
      finalEvaled = transform(list, [this, &itemIdx, &op](const Json& data) {
        return _evalRet(op.cond.get(), strJoin(itemIdx++), data);
      });
      break;
    default:
      break;
  }
  stackReturn(move(finalEvaled), op);
}

void SyntaxEvaluatorImpl::eval(const FunctionInvocation& func) {
  evalThrowIf<SyntaxError>(func.name.empty(),
                           "FunctionInvocation name must not be empty");
  JsonAdapter funcRet;
  evaluateVariables(func.stackVariables);

  if (func.name == StringView{keyword::return_ + 1}) {
    stackReturn(_evalRet(func.param.get()));
  } else {
    auto evaledParam = _evalRet(func.param.get());
    try {
      funcRet = cif::invoke(func.moduleName, func.name,
                            evaledParam ? *evaledParam : JsonAdapter{});
    } catch (const FunctionNotFoundError&) {
      funcRet = stackTopContext()->invoke(
          func.name, evaledParam ? *evaledParam : JsonAdapter{});
    }
    stackReturn(makeEvaluatedVal(move(funcRet)), func);
  }
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
        auto& varInfo = *itProp;
        evalThrowIf<EvaluationError>(varInfo.state == VariableInfo::Evaluating,
                                     "Cyclic reference detected on variable: $",
                                     variableName);
        val = evaluateSingleVar(it->context, variableName, varInfo);
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
      if (!isType<DirectVal>(e)) {
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
    setContextProperty(stackTopContext(), ev.id, val);
  }
  stackReturn(move(val));
}

void jas::SyntaxEvaluatorImpl::stackDebugReturnVal(const EvaluatedValue& e,
                                                   const Evaluable* evb) {
  if (dbCallback_ && evb) {
    auto syntax = syntaxOf(*evb);
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
}

EvaluatedValue& SyntaxEvaluatorImpl::stackReturnedVal() {
  return stackTopFrame().returnedValue;
}

EvaluatedValue jas::SyntaxEvaluatorImpl::stackTakeReturnedVal() {
  EvaluatedValue e = move(stackReturnedVal());
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

const jas::EvalContextPtr& jas::SyntaxEvaluatorImpl::stackRootContext() {
  return stack_.front().context;
}

jas::EvaluatedValue jas::SyntaxEvaluatorImpl::evaluateSingleVar(
    const EvalContextPtr& ctxt, const String& varname, const VariableInfo& vi) {
  vi.state = VariableInfo::Evaluating;
  auto val = _evalRet(vi.variable.get());
  if (vi.type == VariableInfo::Declaration) {
    setContextProperty(ctxt, varname, val);
  } else if (auto varVal = ctxt->property(varname); varVal) {
    if (val.use_count() == 1) {
      using namespace std;
      swap(*val, *varVal);
      val = varVal;
    } else {
      *varVal = *val;
    }
  }
  vi.state = VariableInfo::Evaluated;
  return val;
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
EvaluatedValue SyntaxEvaluatorImpl::applyOp(const _Params& frame) {
  EvaluatedValue firstEvaluated = frame.front();
  return firstEvaluated->visitValue(
      [&frame, this](auto&& val) {
        using ValType =
            std::remove_const_t<std::remove_reference_t<decltype(val)>>;
        if constexpr (op_traits::operationSupported<ValType>(_optype_val)) {
          return _applier_impl<ValType, _std_op<ValType>, _Params>::apply(
              frame);
        } else {
          evalThrow<EvaluationError>("Operator ", _optype_val,
                                     " is not applicable on type `",
                                     typeid(val).name(), "`");
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

template <class T, T _optype, template <class> class _std_op, class _Params>
EvaluatedValue SyntaxEvaluatorImpl::applySingleBinOp(const _Params& ievals) {
  return applyOp<T, _optype, _std_op, ApplySingleBinOpImpl>(ievals);
}

template <class T, T _optype, template <class> class _std_op, class _Params>
EvaluatedValue SyntaxEvaluatorImpl::applyMultiBinOp(const _Params& ievals) {
  return applyOp<T, _optype, _std_op, ApplyMutiBinOpImpl>(ievals);
}

template <class T, T _optype, template <class> class _std_op, class _Params>
EvaluatedValue SyntaxEvaluatorImpl::applyUnaryOp(const _Params& ievals) {
  return applyOp<T, _optype, _std_op, ApplyUnaryOpImpl>(ievals);
}
template <class _Params>
EvaluatedValue SyntaxEvaluatorImpl::applyLogicalAndOp(const _Params& ievals) {
  return applyOp<lot, lot::logical_and, std::logical_and,
                 ApplyLogicalAndOpImpl>(ievals);
}
template <class _Params>
EvaluatedValue SyntaxEvaluatorImpl::applyLogicalOrOp(const _Params& ievals) {
  return applyOp<lot, lot::logical_or, std::logical_or, ApplyLogicalOrOpImpl>(
      ievals);
}
}  // namespace jas
