#pragma once

#include <functional>
#include <memory>
#include <variant>
#include <vector>

#include "jas_string.h"

namespace jas {

template <class T>
using __remove_cr_t = std::remove_const_t<std::remove_reference_t<T>>;

struct evaluable_base;
struct constant_val;
struct arithmatic_operation;
struct logical_operation;
struct evb_comparison_op;
struct list_operation;
struct function_invoker;
struct context_value_collector;

class evaluator_base {
 public:
  virtual void eval(const constant_val&) = 0;
  virtual void eval(const arithmatic_operation&) = 0;
  virtual void eval(const logical_operation&) = 0;
  virtual void eval(const evb_comparison_op&) = 0;
  virtual void eval(const list_operation&) = 0;
  virtual void eval(const function_invoker&) = 0;
  virtual void eval(const context_value_collector&) = 0;
};

struct evaluable_base {
  virtual ~evaluable_base() = default;
  virtual void accept(class evaluator_base* e) const = 0;
};

using evaluable_ptr = std::shared_ptr<evaluable_base>;
using evaluable_list_t = std::vector<evaluable_ptr>;
template <class _operand>
struct evaluable_t : public evaluable_base {
  void accept(evaluator_base* e) const override {
    e->eval(static_cast<const _operand&>(*this));
  }
};

struct constant_val : public evaluable_t<constant_val> {
  struct null_t {};
  using value_t = std::variant<nullptr_t, string_type, bool, int64_t, double>;

  constant_val(value_t v = nullptr_t{}) : value{std::move(v)} {}
  constant_val(const constant_val&) = default;
  constant_val(constant_val&&) = default;
  constant_val& operator=(const constant_val&) = default;
  constant_val& operator=(constant_val&&) = default;

  template <class T>
  T get() const {
    if constexpr (std::is_same_v<bool, std::decay_t<T>>) {
      return std::get<bool>(value);
    } else if constexpr (std::is_integral_v<T>) {
      return static_cast<T>(std::get<int64_t>(value));
    } else {
      return std::get<T>(value);
    }
  }

  template <typename T>
  bool is_type() const {
    return std::holds_alternative<T>(value);
  }

  value_t value;
};

template <class _type>
struct __operator_base {
  using param_t = evaluable_base;
  using param_ptr = evaluable_ptr;
  using param_list_t = evaluable_list_t;
  using operator_t = _type;

  __operator_base(operator_t t, param_list_t p)
      : type{t}, params{std::move(p)} {}
  void add_param(param_ptr p) { params.push_back(std::move(p)); }

  operator_t type;
  param_list_t params;
};

enum class arithmetic_operator_type : int32_t {
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

inline string_type to_string(arithmetic_operator_type o) {
  switch (o) {
    case arithmetic_operator_type::plus:
      return "+";
    case arithmetic_operator_type::minus:
      return "-";
    case arithmetic_operator_type::multiplies:
      return "*";
    case arithmetic_operator_type::divides:
      return "/";
    case arithmetic_operator_type::modulus:
      return "%";
    case arithmetic_operator_type::bit_and:
      return "&";
    case arithmetic_operator_type::bit_or:
      return "|";
    case arithmetic_operator_type::bit_not:
      return "~";
    case arithmetic_operator_type::bit_xor:
      return "xor";
    case arithmetic_operator_type::negate:
      return "-";
    case arithmetic_operator_type::invalid:
      return "invalid";
  }
  return "invalid";
}

struct arithmatic_operation : __operator_base<arithmetic_operator_type>,
                              public evaluable_t<arithmatic_operation> {
  using __operator_base<arithmetic_operator_type>::__operator_base;
};

enum class logical_operation_type : char {
  logical_and,
  logical_or,
  logical_not,
  invalid,
};

inline string_type to_string(logical_operation_type o) {
  switch (o) {
    case logical_operation_type::logical_and:
      return "&&";
    case logical_operation_type::logical_or:
      return "||";
    case logical_operation_type::logical_not:
      return "!";
    case logical_operation_type::invalid:
      return "invalid";
  }
  return "invalid";
}

struct logical_operation : __operator_base<logical_operation_type>,
                           public evaluable_t<logical_operation> {
  using __operator_base<logical_operation_type>::__operator_base;
};

enum class comparison_operator_type : char {
  eq,
  neq,
  lt,
  gt,
  le,
  ge,
  invalid,
};

inline string_type to_string(comparison_operator_type o) {
  switch (o) {
    case comparison_operator_type::eq:
      return "==";
    case comparison_operator_type::neq:
      return "!=";
    case comparison_operator_type::lt:
      return "<";
    case comparison_operator_type::gt:
      return ">";
    case comparison_operator_type::le:
      return "<=";
    case comparison_operator_type::ge:
      return ">=";
    case comparison_operator_type::invalid:
      return "invalid";
  }
  return "invalid";
}

struct evb_comparison_op : __operator_base<comparison_operator_type>,
                           public evaluable_t<evb_comparison_op> {
  using __operator_base<comparison_operator_type>::__operator_base;
};

enum class list_operation_type : char {
  any_of,
  all_of,
  none_of,
  size_of,
  invalid,
};

inline string_type to_string(list_operation_type o) {
  switch (o) {
    case list_operation_type::any_of:
      return "any_of";
    case list_operation_type::all_of:
      return "all_of";
    case list_operation_type::none_of:
      return "none_of";
    case list_operation_type::size_of:
      return "size_of";
    case list_operation_type::invalid:
      return "invalid";
  }
  return "invalid";
}

struct list_operation : public evaluable_t<list_operation> {
  using condition_ptr = evaluable_ptr;

  list_operation(list_operation_type t, condition_ptr c, string_type iid = {},
                 string_type _key = {})
      : type(t),
        list_path(std::move(_key)),
        item_id{std::move(iid)},
        cond(std::move(c)) {}

  list_operation_type type;
  string_type list_path;
  string_type item_id;
  condition_ptr cond;
};

struct function_invoker : public evaluable_t<function_invoker> {
  function_invoker(string_type func_name, evaluable_ptr param)
      : func_name(std::move(func_name)), param(std::move(param)) {}

  string_type func_name;
  evaluable_ptr param;
};

struct context_value_collector : public evaluable_t<context_value_collector> {
  enum snapshot_idx_val : size_t { old_snapshot_idx = 0, new_snapshot_idx = 1 };

  context_value_collector(string_type path, size_t idx = new_snapshot_idx)
      : value_path{std::move(path)}, snapshot_idx{idx} {}
  string_type value_path;
  size_t snapshot_idx = new_snapshot_idx;
};

inline auto make_op(arithmetic_operator_type op, evaluable_list_t params) {
  return std::make_shared<arithmatic_operation>(op, std::move(params));
}
inline auto make_op(logical_operation_type op, evaluable_list_t params) {
  return std::make_shared<logical_operation>(op, std::move(params));
}
inline auto make_op(comparison_operator_type op, evaluable_list_t params) {
  return std::make_shared<evb_comparison_op>(op, std::move(params));
}
inline auto make_op(list_operation_type op, evaluable_ptr cond,
                    string_type iid = {}, string_type key = {}) {
  return std::make_shared<list_operation>(op, std::move(cond), std::move(iid),
                                          std::move(key));
}

inline auto make_const(int val) {
  return std::make_shared<constant_val>(static_cast<int64_t>(val));
}
inline auto make_const(int64_t val) {
  return std::make_shared<constant_val>(val);
}
inline auto make_const(float val) {
  return std::make_shared<constant_val>(static_cast<double>(val));
}
inline auto make_const(double val) {
  return std::make_shared<constant_val>(val);
}
inline auto make_const(string_type val) {
  return std::make_shared<constant_val>(std::move(val));
}
inline auto make_const(bool val) { return std::make_shared<constant_val>(val); }

inline auto make_const(nullptr_t val) {
  return std::make_shared<constant_val>(val);
}
inline auto make_ctxt_value_collector(
    string_type val,
    size_t snapshot_idx = context_value_collector::new_snapshot_idx) {
  return std::make_shared<context_value_collector>(std::move(val),
                                                   snapshot_idx);
}

inline auto make_func(string_type func_name, evaluable_ptr param = {}) {
  return std::make_shared<function_invoker>(std::move(func_name),
                                            std::move(param));
}

inline auto operator"" _const(uint64_t v) {
  return std::make_shared<constant_val>(static_cast<int64_t>(v));
}

inline auto operator"" _const(long double v) {
  return std::make_shared<constant_val>(static_cast<double>(v));
}
inline auto operator"" _const(const char* v, size_t s) {
  return std::make_shared<constant_val>(std::string(v, s));
}

}  // namespace jas
