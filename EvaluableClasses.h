#pragma once

#include <functional>
#include <map>
#include <memory>
#include <variant>
#include <vector>

#include "Evaluable.h"
#include "Json.h"
#include "String.h"

namespace jas {

struct Evaluable;
struct DirectVal;
struct EvaluableMap;
struct EvaluableArray;
struct ArithmaticalOperator;
struct LogicalOperator;
struct ComparisonOperator;
struct ListOperation;
struct Function;
struct ContextVal;
struct Property;
using Evaluables = std::vector<EvaluablePtr>;

class EvaluatorBase {
 public:
  virtual ~EvaluatorBase() = default;
  virtual void eval(const DirectVal&) = 0;
  virtual void eval(const EvaluableMap&) = 0;
  virtual void eval(const EvaluableArray&) = 0;
  virtual void eval(const ArithmaticalOperator&) = 0;
  virtual void eval(const LogicalOperator&) = 0;
  virtual void eval(const ComparisonOperator&) = 0;
  virtual void eval(const ListOperation&) = 0;
  virtual void eval(const Function&) = 0;
  virtual void eval(const ContextVal&) = 0;
  virtual void eval(const Property&) = 0;
};

template <class _SpecEvaluable>
struct __BeEvaluable : public Evaluable {
  __BeEvaluable(String id = {}) : Evaluable(std::move(id)) {}
  void accept(EvaluatorBase* e) const override {
    e->eval(static_cast<const _SpecEvaluable&>(*this));
  }
};

struct DirectVal : public __BeEvaluable<DirectVal> {
  template <class T,
            std::enable_if_t<JsonTrait::isJsonConstructible<T>(), bool> = true>
  DirectVal(T&& v) : value{JsonTrait::makeJson(std::forward<T>(v))} {}
  DirectVal(Json v) : value{std::move(v)} {}
  DirectVal() = default;
  DirectVal(const DirectVal&) = default;
  DirectVal(DirectVal&&) noexcept = default;
  DirectVal& operator=(const DirectVal&) = default;
  DirectVal& operator=(DirectVal&&) = default;

  template <class T>
  bool isType() const {
    return JsonTrait::isType<T>(value);
  }
  template <class T>
  T get() const {
    if constexpr (std::is_same_v<T, Json>) {
      return value;
    } else {
      return JsonTrait::get<T>(value);
    }
  }

  template <class T>
  operator T() const {
    return get<T>();
  }

  operator const Json&() const { return value; }

  template <typename _callable>
  auto visitValue(_callable&& apply) const {
    if (isType<bool>()) {
      return apply(get<bool>());
    } else if (isType<int>()) {
      return apply(get<int>());
    } else if (isType<double>()) {
      return apply(get<double>());
    } else if (isType<String>()) {
      return apply(get<String>());
    }
    return apply(value);
  }

  Json value;
};

struct EvaluableMap : public __BeEvaluable<EvaluableMap> {
  using value_type = std::map<String, EvaluablePtr>;
  EvaluableMap(value_type v = {}) : value{std::move(v)} {}
  value_type value;
};

struct EvaluableArray : public __BeEvaluable<EvaluableArray> {
  using value_type = std::vector<EvaluablePtr>;
  EvaluableArray(value_type v = {}) : value{std::move(v)} {}
  value_type value;
};

template <class _SubType, class _Type>
struct _OperatorBase : public __BeEvaluable<_SubType> {
  using Param = Evaluable;
  using ParamPtr = EvaluablePtr;
  using Params = Evaluables;
  using OperatorType = _Type;

  _OperatorBase(String id, OperatorType t, Params p)
      : __BeEvaluable<_SubType>(std::move(id)), type{t}, params{std::move(p)} {}
  void add_param(ParamPtr p) { params.push_back(std::move(p)); }

  OperatorType type;
  Params params;
};

enum class ArithmeticOperatorType : int32_t {
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

inline OStream& operator<<(OStream& os, ArithmeticOperatorType o) {
  switch (o) {
    case ArithmeticOperatorType::plus:
      os << "+";
      break;
    case ArithmeticOperatorType::minus:
      os << "-";
      break;
    case ArithmeticOperatorType::multiplies:
      os << "*";
      break;
    case ArithmeticOperatorType::divides:
      os << "/";
      break;
    case ArithmeticOperatorType::modulus:
      os << "%";
      break;
    case ArithmeticOperatorType::bit_and:
      os << "&";
      break;
    case ArithmeticOperatorType::bit_or:
      os << "|";
      break;
    case ArithmeticOperatorType::bit_not:
      os << "~";
      break;
    case ArithmeticOperatorType::bit_xor:
      os << "xor";
      break;
    case ArithmeticOperatorType::negate:
      os << "-";
      break;
    case ArithmeticOperatorType::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ArithmaticalOperator
    : public _OperatorBase<ArithmaticalOperator, ArithmeticOperatorType> {
  using _OperatorBase<ArithmaticalOperator,
                      ArithmeticOperatorType>::_OperatorBase;
};

enum class LogicalOperatorType : char {
  logical_and,
  logical_or,
  logical_not,
  invalid,
};

inline OStream& operator<<(OStream& os, LogicalOperatorType o) {
  switch (o) {
    case LogicalOperatorType::logical_and:
      os << "&&";
      break;
    case LogicalOperatorType::logical_or:
      os << "||";
      break;
    case LogicalOperatorType::logical_not:
      os << "!";
      break;
    case LogicalOperatorType::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct LogicalOperator : _OperatorBase<LogicalOperator, LogicalOperatorType> {
  using _OperatorBase<LogicalOperator, LogicalOperatorType>::_OperatorBase;
};

enum class ComparisonOperatorType : char {
  eq,
  neq,
  lt,
  gt,
  le,
  ge,
  invalid,
};

inline OStream& operator<<(OStream& os, ComparisonOperatorType o) {
  switch (o) {
    case ComparisonOperatorType::eq:
      os << "==";
      break;
    case ComparisonOperatorType::neq:
      os << "!=";
      break;
    case ComparisonOperatorType::lt:
      os << "<";
      break;
    case ComparisonOperatorType::gt:
      os << ">";
      break;
    case ComparisonOperatorType::le:
      os << "<=";
      break;
    case ComparisonOperatorType::ge:
      os << ">=";
      break;
    case ComparisonOperatorType::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ComparisonOperator
    : _OperatorBase<ComparisonOperator, ComparisonOperatorType> {
  using _OperatorBase<ComparisonOperator,
                      ComparisonOperatorType>::_OperatorBase;
};

enum class ListOperationType : char {
  any_of,
  all_of,
  none_of,
  size_of,
  count_if,
  invalid,
};

inline OStream& operator<<(OStream& os, ListOperationType o) {
  switch (o) {
    case ListOperationType::any_of:
      os << "any_of";
      break;
    case ListOperationType::all_of:
      os << "all_of";
      break;
    case ListOperationType::none_of:
      os << "none_of";
      break;
    case ListOperationType::size_of:
      os << "size_of";
      break;
    case ListOperationType::count_if:
      os << "count_if";
      break;
    case ListOperationType::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ListOperation : public __BeEvaluable<ListOperation> {
  ListOperation(String id, ListOperationType t, EvaluablePtr c, String iid = {},
                EvaluablePtr list = {})
      : __BeEvaluable<ListOperation>(std::move(id)),
        type(t),
        list(std::move(list)),
        itemID{std::move(iid)},
        cond(std::move(c)) {}

  ListOperationType type;
  String path;
  EvaluablePtr list;
  String itemID;
  EvaluablePtr cond;
};

struct Function : public __BeEvaluable<Function> {
  Function(String id, String name, EvaluablePtr param)
      : __BeEvaluable<Function>(std::move(id)),
        name(std::move(name)),
        param(std::move(param)) {}

  String name;
  EvaluablePtr param;
};

struct ContextVal : public __BeEvaluable<ContextVal> {
  enum Snapshot : size_t {
    old = 0,
    new_ = 1,
    MaxIdx,
  };

  ContextVal(String id, String path, size_t idx = new_)
      : __BeEvaluable<ContextVal>(std::move(id)),
        path{std::move(path)},
        snapshot{idx} {}
  String path;
  Snapshot snapshot = new_;
};

struct Property : public __BeEvaluable<Property> {
  using _Base = __BeEvaluable<Property>;
  using _Base::_Base;
};

inline auto makeOp(String id, ArithmeticOperatorType op, Evaluables params) {
  return std::make_shared<ArithmaticalOperator>(std::move(id), op,
                                                std::move(params));
}
inline auto makeOp(String id, LogicalOperatorType op, Evaluables params) {
  return std::make_shared<LogicalOperator>(std::move(id), op,
                                           std::move(params));
}
inline auto makeOp(String id, ComparisonOperatorType op, Evaluables params) {
  return std::make_shared<ComparisonOperator>(std::move(id), op,
                                              std::move(params));
}
inline auto makeOp(String id, ListOperationType op, EvaluablePtr cond,
                   String iid = {}, EvaluablePtr list = {}) {
  return std::make_shared<ListOperation>(std::move(id), op, std::move(cond),
                                         std::move(iid), std::move(list));
}

inline auto makeConst(int val) {
  return std::make_shared<DirectVal>(static_cast<int64_t>(val));
}
inline auto makeConst(int64_t val) { return std::make_shared<DirectVal>(val); }
inline auto makeConst(float val) {
  return std::make_shared<DirectVal>(static_cast<double>(val));
}
inline auto makeConst(double val) { return std::make_shared<DirectVal>(val); }
inline auto makeConst(String val) {
  return std::make_shared<DirectVal>(std::move(val));
}
inline auto makeConst(bool val) { return std::make_shared<DirectVal>(val); }
inline auto makeConst(Json val) {
  return std::make_shared<DirectVal>(std::move(val));
}
inline auto makeConst(nullptr_t val) {
  return std::make_shared<DirectVal>(val);
}
inline auto makeFnc(String id, String name, EvaluablePtr param = {}) {
  return std::make_shared<Function>(std::move(id), std::move(name),
                                    std::move(param));
}
inline auto makeCV(String id, String val,
                   size_t snapshot_idx = ContextVal::new_) {
  return std::make_shared<ContextVal>(std::move(id), std::move(val),
                                      snapshot_idx);
}
inline auto makeProp(String propID) {
  return std::make_shared<Property>(std::move(propID));
}

inline auto operator"" _const(uint64_t v) {
  return std::make_shared<DirectVal>(static_cast<int64_t>(v));
}

inline auto operator"" _const(long double v) {
  return std::make_shared<DirectVal>(static_cast<double>(v));
}
inline auto operator"" _const(const char* v, size_t s) {
  return std::make_shared<DirectVal>(std::string(v, s));
}

inline auto makeEMap(EvaluableMap::value_type v = {}) {
  return std::make_shared<EvaluableMap>(std::move(v));
}
inline auto makeEArray(EvaluableArray::value_type v = {}) {
  return std::make_shared<EvaluableArray>(std::move(v));
}

}  // namespace jas
