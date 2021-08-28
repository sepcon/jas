#pragma once

#include <map>
#include <memory>
#include <vector>

#include "Evaluable.h"
#include "Exception.h"
#include "FunctionModule.h"
#include "Json.h"
#include "String.h"
#include "Var.h"

namespace jas {

class Evaluable;
struct Constant;
struct EvaluableDict;
struct EvaluableList;
struct ArithmaticalOperator;
struct ArthmSelfAssignOperator;
struct LogicalOperator;
struct ComparisonOperator;
struct ListAlgorithm;
struct ModuleFI;
struct ContextFI;
struct EvaluatorFI;
struct VariableFieldQuery;
struct Variable;
using Evaluables = std::vector<EvaluablePtr>;
struct VariableInfo;
using StackVariables = std::set<VariableInfo, std::less<>>;
using StackVariablesPtr = std::shared_ptr<StackVariables>;

struct VariableInfo {
  enum State { NotEvaluated, Evaluating, Evaluated };
  enum Type { Declaration, Update };
  VariableInfo(String n, EvaluablePtr v, Type tp = Declaration)
      : name(std::move(n)), variable(std::move(v)), type(tp) {}

  String name;
  EvaluablePtr variable;
  Type type;
  mutable State state = NotEvaluated;
};

inline bool operator<(const VariableInfo& first, const VariableInfo& second) {
  return (first.name < second.name) ||
         ((first.name == second.name) && (first.type < second.type));
}

inline bool operator<(const VariableInfo& vi, const String& name) {
  return vi.name < name;
}
inline bool operator<(const String& name, const VariableInfo& vi) {
  return name < vi.name;
}

class EvaluatorBase {
 public:
  virtual ~EvaluatorBase() = default;
  virtual void eval(const Constant&) = 0;
  virtual void eval(const EvaluableDict&) = 0;
  virtual void eval(const EvaluableList&) = 0;
  virtual void eval(const ArithmaticalOperator&) = 0;
  virtual void eval(const ArthmSelfAssignOperator&) = 0;
  virtual void eval(const LogicalOperator&) = 0;
  virtual void eval(const ComparisonOperator&) = 0;
  virtual void eval(const ListAlgorithm&) = 0;
  virtual void eval(const ModuleFI&) = 0;
  virtual void eval(const ContextFI&) = 0;
  virtual void eval(const EvaluatorFI&) = 0;
  virtual void eval(const VariableFieldQuery&) = 0;
  virtual void eval(const Variable&) = 0;
};

template <class _Evaluable>
bool isType(const Evaluable* evb) {
  if (evb) {
    auto& revb = *evb;  // avoid clang side-effect warning
    return (typeid(revb) == typeid(_Evaluable));
  }
  return false;
}

template <class _Evaluable>
bool isType(const EvaluablePtr& evb) {
  return isType<_Evaluable>(evb.get());
}

template <class _SpecEvaluable>
struct __BeEvaluable : public Evaluable {
  __BeEvaluable(String id = {}) : Evaluable(std::move(id)) {}
  void accept(EvaluatorBase* e) const override {
    e->eval(static_cast<const _SpecEvaluable&>(*this));
  }
};

struct __UseStackEvaluable : public Evaluable {
  __UseStackEvaluable(StackVariablesPtr variables, String id = {})
      : Evaluable(std::move(id)), stackVariables{std::move(variables)} {}
  bool useStack() const override { return true; }
  StackVariablesPtr stackVariables;
};

template <class _SpecEvaluable>
struct __UseStackEvaluableT : public __UseStackEvaluable {
  void accept(EvaluatorBase* e) const override {
    e->eval(static_cast<const _SpecEvaluable&>(*this));
  }
  using __Base = __UseStackEvaluable;
  using __Base::__Base;
};

struct __StacklessEvaluable : public Evaluable {
  __StacklessEvaluable(String id = {}) : Evaluable(std::move(id)) {}
  bool useStack() const override { return false; }
};

template <class _SpecEvaluable>
struct __StacklessEvaluableT : public __StacklessEvaluable {
  using __StacklessEvaluable::__StacklessEvaluable;
  void accept(EvaluatorBase* e) const override {
    e->eval(static_cast<const _SpecEvaluable&>(*this));
  }
};

struct Constant : public __StacklessEvaluableT<Constant> {
  template <class T>
  Constant(T&& v) : value(std::forward<T>(v)) {}
  Var value;
};

struct EvaluableDict : public __UseStackEvaluableT<EvaluableDict> {
  using __Base = __UseStackEvaluableT<EvaluableDict>;
  using value_type = std::map<String, EvaluablePtr>;
  EvaluableDict(value_type v = {}, StackVariablesPtr cp = {})
      : __Base(std::move(cp)), value{std::move(v)} {}
  value_type value;
};

struct EvaluableList : public __StacklessEvaluableT<EvaluableList> {
  using value_type = std::vector<EvaluablePtr>;
  EvaluableList(value_type v = {}) : value{std::move(v)} {}
  value_type value;
};

template <class _SubType, class _Type>
struct _OperatorBase : public __UseStackEvaluableT<_SubType> {
  using Param = Evaluable;
  using ParamPtr = EvaluablePtr;
  using Params = Evaluables;
  using OperatorType = _Type;
  _OperatorBase(String id, OperatorType t, Params p, StackVariablesPtr cp)
      : __UseStackEvaluableT<_SubType>(std::move(cp), std::move(id)),
        type{t},
        params{std::move(p)} {}
  void add_param(ParamPtr p) { params.push_back(std::move(p)); }

  OperatorType type;
  Params params;
};

enum class aot : int32_t {
  plus,
  minus,
  multiplies,
  divides,
  modulus,
  bit_and,
  bit_or,
  bit_not,
  bit_xor,
  negate,
  invalid,
};

inline OStream& operator<<(OStream& os, aot o) {
  switch (o) {
    case aot::plus:
      os << "+";
      break;
    case aot::minus:
      os << "-";
      break;
    case aot::multiplies:
      os << "*";
      break;
    case aot::divides:
      os << "/";
      break;
    case aot::modulus:
      os << "%";
      break;
    case aot::bit_and:
      os << "&";
      break;
    case aot::bit_or:
      os << "|";
      break;
    case aot::bit_not:
      os << "~";
      break;
    case aot::bit_xor:
      os << "xor";
      break;
    case aot::negate:
      os << "-";
      break;
    case aot::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ArithmaticalOperator : public _OperatorBase<ArithmaticalOperator, aot> {
  using _OperatorBase<ArithmaticalOperator, aot>::_OperatorBase;
};

enum class asot : int32_t {
  s_plus,
  s_minus,
  s_multiplies,
  s_divides,
  s_modulus,
  s_bit_and,
  s_bit_or,
  s_bit_xor,
  invalid,
};

inline OStream& operator<<(OStream& os, asot o) {
  switch (o) {
    case asot::s_plus:
      os << "+=";
      break;
    case asot::s_minus:
      os << "-=";
      break;
    case asot::s_multiplies:
      os << "*=";
      break;
    case asot::s_divides:
      os << "/=";
      break;
    case asot::s_modulus:
      os << "%=";
      break;
    case asot::s_bit_and:
      os << "&=";
      break;
    case asot::s_bit_or:
      os << "|=";
      break;
    case asot::s_bit_xor:
      os << "xor=";
      break;
    case asot::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ArthmSelfAssignOperator
    : public _OperatorBase<ArthmSelfAssignOperator, asot> {
  using _OperatorBase<ArthmSelfAssignOperator, asot>::_OperatorBase;
};

enum class lot : char {
  logical_and,
  logical_or,
  logical_not,
  invalid,
};

inline OStream& operator<<(OStream& os, lot o) {
  switch (o) {
    case lot::logical_and:
      os << "&&";
      break;
    case lot::logical_or:
      os << "||";
      break;
    case lot::logical_not:
      os << "!";
      break;
    case lot::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct LogicalOperator : _OperatorBase<LogicalOperator, lot> {
  using _OperatorBase<LogicalOperator, lot>::_OperatorBase;
};

enum class cot : char {
  eq,
  neq,
  lt,
  gt,
  le,
  ge,
  invalid,
};

inline OStream& operator<<(OStream& os, cot o) {
  switch (o) {
    case cot::eq:
      os << "==";
      break;
    case cot::neq:
      os << "!=";
      break;
    case cot::lt:
      os << "<";
      break;
    case cot::gt:
      os << ">";
      break;
    case cot::le:
      os << "<=";
      break;
    case cot::ge:
      os << ">=";
      break;
    case cot::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ComparisonOperator : _OperatorBase<ComparisonOperator, cot> {
  using _OperatorBase<ComparisonOperator, cot>::_OperatorBase;
};

enum class lsaot : char {
  any_of,
  all_of,
  none_of,
  count_if,
  filter_if,
  transform,
  invalid,
};

inline OStream& operator<<(OStream& os, lsaot o) {
  switch (o) {
    case lsaot::any_of:
      os << "any_of";
      break;
    case lsaot::all_of:
      os << "all_of";
      break;
    case lsaot::none_of:
      os << "none_of";
      break;
    case lsaot::count_if:
      os << "count_if";
      break;
    case lsaot::filter_if:
      os << "filter_if";
      break;
    case lsaot::transform:
      os << "transform";
      break;
    case lsaot::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ListAlgorithm : public __UseStackEvaluableT<ListAlgorithm> {
  ListAlgorithm(String id, lsaot t, EvaluablePtr c, EvaluablePtr list = {},
                StackVariablesPtr cp = {})
      : __UseStackEvaluableT<ListAlgorithm>(std::move(cp), std::move(id)),
        type(t),
        list(std::move(list)),
        cond(std::move(c)) {}

  lsaot type;
  EvaluablePtr list;
  EvaluablePtr cond;
};

template <class _SpecificFI>
struct FunctionInvocationBase : public __UseStackEvaluableT<_SpecificFI> {
  FunctionInvocationBase(String id, String name, EvaluablePtr param,
                         StackVariablesPtr cp = {})
      : __UseStackEvaluableT<_SpecificFI>(std::move(cp), std::move(id)),
        name(std::move(name)),
        param(std::move(param)) {}

  String name;
  EvaluablePtr param;
};

struct ContextFI : public FunctionInvocationBase<ContextFI> {
  using _Base = FunctionInvocationBase<ContextFI>;
  using _Base::_Base;
};

struct ModuleFI : public FunctionInvocationBase<ModuleFI> {
  using _Base = FunctionInvocationBase<ModuleFI>;
  ModuleFI(String id, String name, EvaluablePtr param, FunctionModulePtr mdl,
           StackVariablesPtr cp = {})
      : _Base(move(id), move(name), move(param), std::move(cp)),
        module(std::move(mdl)) {}
  FunctionModulePtr module;
};

struct EvaluatorFI : public FunctionInvocationBase<EvaluatorFI> {
  using _Base = FunctionInvocationBase<EvaluatorFI>;
  using _Base::_Base;
};

struct VariableFieldQuery : public __StacklessEvaluableT<VariableFieldQuery> {
  using _Base = __StacklessEvaluableT<VariableFieldQuery>;
  VariableFieldQuery(String name, std::vector<EvaluablePtr> path)
      : _Base(std::move(name)), field_path(std::move(path)) {}

  std::vector<EvaluablePtr> field_path;
};

struct Variable : public __StacklessEvaluableT<Variable> {
  using _Base = __StacklessEvaluableT<Variable>;
  using _Base::_Base;
};

template <class T>
inline auto makeConst(T&& val) {
  return std::make_shared<Constant>(std::forward<T>(val));
}
inline auto makeOp(String id, aot op, Evaluables params,
                   StackVariablesPtr cp = {}) {
  return std::make_shared<ArithmaticalOperator>(
      std::move(id), op, std::move(params), std::move(cp));
}
inline auto makeOp(String id, asot op, Evaluables params,
                   StackVariablesPtr cp = {}) {
  return std::make_shared<ArthmSelfAssignOperator>(
      std::move(id), op, std::move(params), std::move(cp));
}
inline auto makeOp(String id, lot op, Evaluables params,
                   StackVariablesPtr cp = {}) {
  return std::make_shared<LogicalOperator>(std::move(id), op, std::move(params),
                                           std::move(cp));
}
inline auto makeOp(String id, cot op, Evaluables params,
                   StackVariablesPtr cp = {}) {
  return std::make_shared<ComparisonOperator>(std::move(id), op,
                                              std::move(params), std::move(cp));
}
inline auto makeOp(String id, lsaot op, EvaluablePtr cond, EvaluablePtr list,
                   StackVariablesPtr cp = {}) {
  return std::make_shared<ListAlgorithm>(std::move(id), op, std::move(cond),
                                         std::move(list), std::move(cp));
}

template <class _FI>
inline auto makeSimpleFI(String id, String name, EvaluablePtr param = {},
                         StackVariablesPtr cp = {}) {
  return std::make_shared<_FI>(std::move(id), std::move(name), std::move(param),
                               std::move(cp));
}

inline auto makeModuleFI(String id, String name, EvaluablePtr param = {},
                         FunctionModulePtr mdl = {},
                         StackVariablesPtr cp = {}) {
  return std::make_shared<ModuleFI>(std::move(id), std::move(name),
                                    std::move(param), std::move(mdl),
                                    std::move(cp));
}

inline auto makeVar(String variableName) {
  return std::make_shared<Variable>(std::move(variableName));
}

inline auto makeVariableFieldQuery(String name,
                                   std::vector<EvaluablePtr> paths) {
  return std::make_shared<VariableFieldQuery>(std::move(name),
                                              std::move(paths));
}

inline auto makeEvbDict(EvaluableDict::value_type v = {}) {
  return std::make_shared<EvaluableDict>(std::move(v));
}

inline auto makeEvbList(EvaluableList::value_type v = {}) {
  return std::make_shared<EvaluableList>(std::move(v));
}

}  // namespace jas
