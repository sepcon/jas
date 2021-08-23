#pragma once

#include <map>
#include <memory>
#include <vector>

#include "Evaluable.h"
#include "Exception.h"
#include "Json.h"
#include "String.h"

namespace jas {

struct Evaluable;
struct DirectVal;
struct EvaluableMap;
struct EvaluableArray;
struct ArithmaticalOperator;
struct ArthmSelfAssignOperator;
struct LogicalOperator;
struct ComparisonOperator;
struct ListOperation;
struct FunctionInvocation;
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
         (first.name == second.name) && (first.type < second.type);
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
  virtual void eval(const DirectVal&) = 0;
  virtual void eval(const EvaluableMap&) = 0;
  virtual void eval(const EvaluableArray&) = 0;
  virtual void eval(const ArithmaticalOperator&) = 0;
  virtual void eval(const ArthmSelfAssignOperator&) = 0;
  virtual void eval(const LogicalOperator&) = 0;
  virtual void eval(const ComparisonOperator&) = 0;
  virtual void eval(const ListOperation&) = 0;
  virtual void eval(const FunctionInvocation&) = 0;
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

struct DirectVal : public __StacklessEvaluableT<DirectVal>, public JsonAdapter {
  using Base = JsonAdapter;
  using Base::Base;
};

struct EvaluableMap : public __UseStackEvaluableT<EvaluableMap> {
  using __Base = __UseStackEvaluableT<EvaluableMap>;
  using value_type = std::map<String, EvaluablePtr>;
  EvaluableMap(value_type v = {}, StackVariablesPtr cp = {})
      : __Base(std::move(cp)), value{std::move(v)} {}
  value_type value;
};

struct EvaluableArray : public __StacklessEvaluableT<EvaluableArray> {
  using value_type = std::vector<EvaluablePtr>;
  EvaluableArray(value_type v = {}) : value{std::move(v)} {}
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

enum class lsot : char {
  any_of,
  all_of,
  none_of,
  count_if,
  filter_if,
  transform,
  invalid,
};

inline OStream& operator<<(OStream& os, lsot o) {
  switch (o) {
    case lsot::any_of:
      os << "any_of";
      break;
    case lsot::all_of:
      os << "all_of";
      break;
    case lsot::none_of:
      os << "none_of";
      break;
    case lsot::count_if:
      os << "count_if";
      break;
    case lsot::filter_if:
      os << "filter_if";
      break;
    case lsot::transform:
      os << "transform";
      break;
    case lsot::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ListOperation : public __UseStackEvaluableT<ListOperation> {
  ListOperation(String id, lsot t, EvaluablePtr c, EvaluablePtr list = {},
                StackVariablesPtr cp = {})
      : __UseStackEvaluableT<ListOperation>(std::move(cp), std::move(id)),
        type(t),
        list(std::move(list)),
        cond(std::move(c)) {}

  lsot type;
  EvaluablePtr list;
  EvaluablePtr cond;
};

struct FunctionInvocation : public __UseStackEvaluableT<FunctionInvocation> {
  FunctionInvocation(String id, String name, EvaluablePtr param, String mdl,
           StackVariablesPtr cp = {})
      : __UseStackEvaluableT<FunctionInvocation>(std::move(cp), std::move(id)),
        name(std::move(name)),
        param(std::move(param)),
        moduleName(std::move(mdl)) {}

  String name;
  EvaluablePtr param;
  String moduleName;
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

inline auto makeDV(int32_t val) {
  return std::make_shared<DirectVal>(static_cast<int64_t>(val));
}
inline auto makeDV(int64_t val) { return std::make_shared<DirectVal>(val); }
inline auto makeDV(float val) {
  return std::make_shared<DirectVal>(static_cast<double>(val));
}
inline auto makeDV(double val) { return std::make_shared<DirectVal>(val); }
inline auto makeDV(String val) {
  return std::make_shared<DirectVal>(std::move(val));
}
inline auto makeDV(bool val) { return std::make_shared<DirectVal>(val); }
inline auto makeDV(Json val) {
  return std::make_shared<DirectVal>(std::move(val));
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
inline auto makeOp(String id, lsot op, EvaluablePtr cond, EvaluablePtr list,
                   StackVariablesPtr cp = {}) {
  return std::make_shared<ListOperation>(std::move(id), op, std::move(cond),
                                         std::move(list), std::move(cp));
}
inline auto makeFnc(String id, String name, EvaluablePtr param = {},
                    String mdl = {}, StackVariablesPtr cp = {}) {
  return std::make_shared<FunctionInvocation>(std::move(id), std::move(name),
                                    std::move(param), std::move(mdl),
                                    std::move(cp));
}
inline auto makeProp(String propID) {
  return std::make_shared<Variable>(std::move(propID));
}
inline auto makeVariableFieldQuery(String name,
                                   std::vector<EvaluablePtr> paths) {
  return std::make_shared<VariableFieldQuery>(std::move(name),
                                              std::move(paths));
}
inline auto makeEMap(EvaluableMap::value_type v = {}) {
  return std::make_shared<EvaluableMap>(std::move(v));
}
inline auto makeEArray(EvaluableArray::value_type v = {}) {
  return std::make_shared<EvaluableArray>(std::move(v));
}

}  // namespace jas
