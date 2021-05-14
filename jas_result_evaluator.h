#pragma once

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stack>
#include <string>

#include "jas_evaluables.h"
#include "jas_exception.h"
#include "jas_string.h"

namespace jas {

class evaluation_context_base;
using evaluated_value = constant_val;
using evaluated_values = std::vector<evaluated_value>;
using evaluation_context_ptr = std::shared_ptr<evaluation_context_base>;
using evaluation_contexts = std::vector<evaluation_context_ptr>;
struct evaluation_frame {
  evaluation_context_ptr context;
  evaluated_values evaluated_vals;
  string_type debug_info;
};
using evaluation_stack = std::list<evaluation_frame>;

class evaluation_context_base {
 public:
  virtual ~evaluation_context_base() = default;
  virtual evaluated_value invoke_function(const string_type& function_name,
                                          evaluated_value param) = 0;
  virtual evaluated_value extract_value(const string_type& value_path,
                                        size_t snapshot_idx) = 0;
  virtual evaluation_contexts extract_sub_contexts(
      const string_type& list_path, const string_type& item_id = {}) = 0;
  virtual string_type dump() const { return "Context NONE"; }
};

class syntax_evaluator : public evaluator_base {
 public:
  syntax_evaluator(evaluation_context_ptr root_context);
  evaluated_value evaluated_result() const;
  void eval(const constant_val& v) override;
  void eval(const arithmatic_operation& op) override;
  void eval(const logical_operation& op) override;
  void eval(const evb_comparison_op& op) override;
  void eval(const list_operation& op) override;
  void eval(const function_invoker& func) override;
  void eval(const context_value_collector& fc) override;

 private:
  /// Exceptions
  template <class _exception>
  void __throw_st_if(bool cond,
                     const string_type& msg = "Unexpected exception");
  template <class _exception>
  void __throw_st(const string_type& msg = "Unexpected exception");

  /// Stack functions
  string_type stack_dump() const;
  void stack_push(string_type debug_info = "__new_stack_frame",
                  evaluation_context_ptr ctxt = {});
  void stack_top_add_val(evaluated_value&& val);
  evaluated_value& stack_top_last_elem();
  evaluation_frame stack_pop();
  evaluation_frame& stack_top_frame();
  const evaluation_context_ptr& stack_top_context();
  string_type stack_trace_msg(const string_type& msg) const;
  evaluated_value evaluate_one(const evaluable_ptr& e,
                               evaluation_context_ptr ctxt = {});
  string_type dump(const evaluable_list_t& l);

  /// Evaluations:
  template <class _operation, class _callable>
  evaluated_value evaluate_operation(const _operation& op,
                                     _callable&& eval_func);
  template <class _compare_method, size_t expected_count, class _operation>
  void validate_param_count(const _operation& o);
  template <class _operation>
  void make_sure_unary_op(const _operation& o);
  template <class T>
  void make_sure_binary_op(const __operator_base<T>& o);
  template <class T>
  void make_sure_single_binary_op(const __operator_base<T>& o);
  template <class _optype_t, _optype_t _optype_val,
            template <class> class _std_op,
            template <class, class> class _applier_impl>
  evaluated_value apply_op(const evaluated_values& frame);
  template <class T, T _optype, template <class> class _std_op>
  evaluated_value apply_single_bin_op(const evaluated_values& frame);
  template <class T, T _optype, template <class> class _std_op>
  evaluated_value apply_multi_bin_op(const evaluated_values& frame);
  template <class T, T _optype, template <class> class _std_op>
  evaluated_value apply_unary_op(const evaluated_values& frame);

  evaluation_stack stack_;
};

namespace _details {
template <class T, typename = bool>
struct operators_supported;

using operators_supported_bits = uint32_t;
static inline constexpr operators_supported_bits __SUPPORT_ALL = 0xFFFFFFFF;
static inline constexpr operators_supported_bits __SUPPORT_NONE = 0;

template <class... _optypes>
inline constexpr operators_supported_bits __ops_support(_optypes... ops) {
  return ((1 << static_cast<operators_supported_bits>(ops)) | ...);
}

template <operators_supported_bits SUPPORTED_AR_LIST = __SUPPORT_ALL,
          operators_supported_bits SUPPORTED_LG_LIST = __SUPPORT_ALL,
          operators_supported_bits SUPPORTED_CMP_LIST = __SUPPORT_ALL>
struct operators_supported_impl {
  template <class _optype>
  static constexpr operators_supported_bits __2bits(_optype opt) {
    return 1 << static_cast<operators_supported_bits>(opt);
  }
  static constexpr bool supported(arithmetic_operator_type opt) {
    return SUPPORTED_AR_LIST & __2bits(opt);
  }
  static constexpr bool supported(logical_operation_type opt) {
    return SUPPORTED_LG_LIST & __2bits(opt);
  }
  static constexpr bool supported(comparison_operator_type opt) {
    return SUPPORTED_CMP_LIST & __2bits(opt);
  }
};

template <>
struct operators_supported<string_type>
    : operators_supported_impl<__ops_support(arithmetic_operator_type::plus),
                               __SUPPORT_NONE, __SUPPORT_ALL> {};

template <>
struct operators_supported<bool>
    : operators_supported_impl<__SUPPORT_NONE, __SUPPORT_ALL, __SUPPORT_ALL> {};

template <typename T>
struct operators_supported<T, std::enable_if_t<std::is_integral_v<T>, bool>>
    : operators_supported_impl<> {};

template <typename T>
struct operators_supported<T,
                           std::enable_if_t<std::is_floating_point_v<T>, bool>>
    : operators_supported_impl<__ops_support(
                                   arithmetic_operator_type::plus,
                                   arithmetic_operator_type::minus,
                                   arithmetic_operator_type::multiplies,
                                   arithmetic_operator_type::divides),
                               __SUPPORT_ALL, __SUPPORT_ALL> {};
template <>
struct operators_supported<nullptr_t>
    : operators_supported_impl<__SUPPORT_NONE, __SUPPORT_NONE, __SUPPORT_NONE> {
};

template <class T, class _optype>
constexpr bool operation_supported(_optype op) {
  return operators_supported<T>::supported(op);
}

class operation_namer : public evaluator_base {
 public:
  string_type name_of(const evaluable_ptr& e) const {
    if (e) {
      e->accept(const_cast<operation_namer*>(this));
      return std::move(evaluated_name_);
    } else {
      return "null_op";
    }
  }

  string_type name_of(const evaluable_base& e) const {
    e.accept(const_cast<operation_namer*>(this));
    return std::move(evaluated_name_);
  }

 private:
  virtual void eval(const constant_val& c) {
    std::visit(
        [this](auto&& v) {
          if constexpr (std::is_same_v<string_type,
                                       __remove_cr_t<decltype(v)>>) {
            _set_name(str_join(std::quoted(v)));
          } else {
            _set_name(str_join(v));
          }
        },
        c.value);
  }

  virtual void eval(const arithmatic_operation& op) {
    _set_name(str_join("operator ", to_string(op.type)));
  }

  virtual void eval(const logical_operation& op) {
    _set_name(str_join("operator ", to_string(op.type)));
  }
  virtual void eval(const evb_comparison_op& op) {
    _set_name(str_join("operator ", to_string(op.type)));
  }
  virtual void eval(const list_operation& op) {
    _set_name(str_join("operation ", to_string(op.type)));
  }
  virtual void eval(const function_invoker& f) {
    ostringstream_type oss;
    oss << "invoke_func( " << f.func_name << "( "
        << (f.param ? this->name_of(f.param) : "") << " ) )";
    evaluated_name_ = oss.str();
  }

  virtual void eval(const context_value_collector& c) {
    ostringstream_type oss;
    oss << "value_of( path=" << c.value_path
        << ", snapshot=" << (c.snapshot_idx == 0 ? "old" : "new") << " )";
    evaluated_name_ = oss.str();
  }

  void _set_name(string_type name) { evaluated_name_ = std::move(name); }

  string_type evaluated_name_;
};

inline operation_namer& op_namer() {
  static operation_namer _;
  return _;
}

inline auto get_name(const evaluable_ptr& e) { return op_namer().name_of(e); }
inline auto get_name(const evaluable_base& e) { return op_namer().name_of(e); }

}  // namespace _details

#define __MC_BASIC_OPERATION_EVAL_START(op) try
#define __MC_BASIC_OPERATION_EVAL_END(op)                             \
  catch (const std::bad_variant_access&) {                            \
    __throw_st<evaluation_error>("parameters of operation `" +        \
                                 to_string(op.type) +                 \
                                 "` must be same type and NOT null"); \
  }

/// Exceptions
template <class _exception>
void syntax_evaluator::__throw_st_if(bool cond, const string_type& msg) {
  if (cond) {
    __throw<_exception>(stack_trace_msg(msg));
  }
}

template <class _exception>
void syntax_evaluator::__throw_st(const string_type& msg) {
  __throw<_exception>(stack_trace_msg(msg));
}

inline syntax_evaluator::syntax_evaluator(evaluation_context_ptr root_context) {
  stack_push("__main_frame__", std::move(root_context));
}

inline evaluated_value syntax_evaluator::evaluated_result() const {
  return stack_.back().evaluated_vals.back();
}

inline void syntax_evaluator::eval(const constant_val& v) {
  stack_.back().evaluated_vals.push_back(v);
}

inline void syntax_evaluator::eval(const arithmatic_operation& op) {
  __MC_BASIC_OPERATION_EVAL_START(op) {
    if ((op.type == arithmetic_operator_type::bit_not) ||
        (op.type == arithmetic_operator_type::negate)) {
      make_sure_unary_op(op);

    } else {
      make_sure_binary_op(op);
    }

    auto e = evaluate_operation(op, [&op, this](auto&& evaluated_vals) {
      switch (op.type) {
        case arithmetic_operator_type::bit_and:
          return apply_multi_bin_op<arithmetic_operator_type,
                                    arithmetic_operator_type::bit_and,
                                    std::bit_and>(evaluated_vals);
        case arithmetic_operator_type::bit_not:
          return apply_unary_op<arithmetic_operator_type,
                                arithmetic_operator_type::bit_not,
                                std::bit_not>(evaluated_vals);
        case arithmetic_operator_type::bit_or:
          return apply_multi_bin_op<arithmetic_operator_type,
                                    arithmetic_operator_type::bit_or,
                                    std::bit_or>(evaluated_vals);
        case arithmetic_operator_type::bit_xor:
          return apply_multi_bin_op<arithmetic_operator_type,
                                    arithmetic_operator_type::bit_xor,
                                    std::bit_xor>(evaluated_vals);
        case arithmetic_operator_type::divides:
          return apply_multi_bin_op<arithmetic_operator_type,
                                    arithmetic_operator_type::divides,
                                    std::divides>(evaluated_vals);
        case arithmetic_operator_type::minus:
          return apply_multi_bin_op<arithmetic_operator_type,
                                    arithmetic_operator_type::minus,
                                    std::minus>(evaluated_vals);
        case arithmetic_operator_type::modulus:
          return apply_multi_bin_op<arithmetic_operator_type,
                                    arithmetic_operator_type::modulus,
                                    std::modulus>(evaluated_vals);
        case arithmetic_operator_type::multiplies:
          return apply_multi_bin_op<arithmetic_operator_type,
                                    arithmetic_operator_type::multiplies,
                                    std::multiplies>(evaluated_vals);
        case arithmetic_operator_type::negate:
          return apply_unary_op<arithmetic_operator_type,
                                arithmetic_operator_type::negate, std::negate>(
              evaluated_vals);
        case arithmetic_operator_type::plus:
          return apply_multi_bin_op<arithmetic_operator_type,
                                    arithmetic_operator_type::plus, std::plus>(
              evaluated_vals);
        default:
          return evaluated_value{};
      }
    });
    stack_top_add_val(std::move(e));
  }
  __MC_BASIC_OPERATION_EVAL_END(op)
}

inline void syntax_evaluator::eval(const logical_operation& op) {
  __MC_BASIC_OPERATION_EVAL_START(op) {
    if (op.type == logical_operation_type::logical_not) {
      make_sure_unary_op(op);
    } else {
      make_sure_binary_op(op);
    }

    auto e = evaluate_operation(op, [&op, this](auto&& evaluated_vals) {
      switch (op.type) {
        case logical_operation_type::logical_and:
          return apply_multi_bin_op<logical_operation_type,
                                    logical_operation_type::logical_and,
                                    std::logical_and>(evaluated_vals);
        case logical_operation_type::logical_or:
          return apply_multi_bin_op<logical_operation_type,
                                    logical_operation_type::logical_or,
                                    std::logical_or>(evaluated_vals);
        case logical_operation_type::logical_not:
          return apply_unary_op<logical_operation_type,
                                logical_operation_type::logical_not,
                                std::logical_not>(evaluated_vals);
        default:
          return evaluated_value{};
      }
    });
    stack_top_add_val(std::move(e));
  }
  __MC_BASIC_OPERATION_EVAL_END(op)
}

inline void syntax_evaluator::eval(const evb_comparison_op& op) {
  __MC_BASIC_OPERATION_EVAL_START(op) {
    make_sure_single_binary_op(op);
    auto e = evaluate_operation(op, [&op, this](auto&& evaluated) {
      switch (op.type) {
        case comparison_operator_type::eq:
          return apply_single_bin_op<comparison_operator_type,
                                     comparison_operator_type::eq,
                                     std::equal_to>(evaluated);
        case comparison_operator_type::gt:
          return apply_single_bin_op<comparison_operator_type,
                                     comparison_operator_type::gt,
                                     std::greater>(evaluated);
        case comparison_operator_type::ge:
          return apply_single_bin_op<comparison_operator_type,
                                     comparison_operator_type::ge,
                                     std::greater_equal>(evaluated);
        case comparison_operator_type::lt:
          return apply_single_bin_op<comparison_operator_type,
                                     comparison_operator_type::lt, std::less>(
              evaluated);
        case comparison_operator_type::le:
          return apply_single_bin_op<comparison_operator_type,
                                     comparison_operator_type::le,
                                     std::less_equal>(evaluated);
        case comparison_operator_type::neq:
          return apply_single_bin_op<comparison_operator_type,
                                     comparison_operator_type::neq,
                                     std::not_equal_to>(evaluated);
        default:
          return evaluated_value{};
      }
    });
    stack_top_add_val(std::move(e));
  }
  __MC_BASIC_OPERATION_EVAL_END(op)
}

inline void syntax_evaluator::eval(const list_operation& op) {
  auto op_name = _details::get_name(op);
  stack_push(op_name);
  auto ctxt_list =
      stack_top_context()->extract_sub_contexts(op.list_path, op.item_id);
  auto eval_impl = [&](const evaluation_context_ptr& ctxt) {
    auto evaluated = evaluate_one(op.cond, ctxt);
    __throw_st_if<evaluation_error>(
        !evaluated.is_type<bool>(),
        str_join("Invalid param type > operation: ", op_name,
                 "` > expected: `boolean` > real_val: `",
                 _details::get_name(evaluated), "`"));
    return evaluated.get<bool>();
  };

  auto final_eval = false;
  switch (op.type) {
    case list_operation_type::any_of:
      final_eval =
          std::any_of(std::begin(ctxt_list), std::end(ctxt_list), eval_impl);
      break;
    case list_operation_type::all_of:
      final_eval =
          std::all_of(std::begin(ctxt_list), std::end(ctxt_list), eval_impl);
      break;
    case list_operation_type::none_of:
      final_eval =
          std::none_of(std::begin(ctxt_list), std::end(ctxt_list), eval_impl);
      break;
    case list_operation_type::size_of:
      final_eval = ctxt_list.size();
      break;
    default:
      break;
  }
  stack_pop();
  stack_top_add_val(evaluated_value{final_eval});
}

inline void syntax_evaluator::eval(const function_invoker& func) {
  __throw_st_if<syntax_error>(
      func.func_name.empty(),
      stack_trace_msg("Function name must not be empty"));
  stack_top_add_val(stack_top_context()->invoke_function(
      func.func_name, evaluate_one(func.param)));
}

inline void syntax_evaluator::eval(const context_value_collector& fc) {
  stack_top_add_val(
      stack_top_context()->extract_value(fc.value_path, fc.snapshot_idx));
}

inline string_type syntax_evaluator::stack_dump() const {
  ostringstream_type oss;
  int i = stack_.size();
  for (auto iframe = std::rbegin(stack_); iframe != std::rend(stack_);
       ++iframe) {
    oss << "#" << i-- << ": " << std::setw(15) << iframe->debug_info
        << std::setw(0) << " ( ";
    auto ival = std::begin(iframe->evaluated_vals);
    auto ival_end = std::end(iframe->evaluated_vals);
    if (ival != ival_end) {
      oss << _details::get_name(*ival);
      while (++ival != ival_end) {
        oss << ", " << _details::get_name(*ival);
      }
    }

    oss << " )"
        << " --> Context: " << iframe->context->dump() << "\n";
  }
  return oss.str();
}

inline void syntax_evaluator::stack_push(string_type debug_info,
                                         evaluation_context_ptr ctxt) {
  if (!ctxt) {
    ctxt = stack_top_context();
  }
  stack_.emplace_back(evaluation_frame{std::move(ctxt), evaluated_values{},
                                       std::move(debug_info)});
}

inline void syntax_evaluator::stack_top_add_val(evaluated_value&& val) {
  stack_top_frame().evaluated_vals.push_back(std::move(val));
}

inline evaluated_value& syntax_evaluator::stack_top_last_elem() {
  return stack_top_frame().evaluated_vals.back();
}

inline evaluation_frame syntax_evaluator::stack_pop() {
  auto top = std::move(stack_.back());
  stack_.pop_back();
  return top;
}

inline evaluation_frame& syntax_evaluator::stack_top_frame() {
  return stack_.back();
}

inline const evaluation_context_ptr& syntax_evaluator::stack_top_context() {
  return stack_.back().context;
}

inline string_type syntax_evaluator::stack_trace_msg(
    const string_type& msg) const {
  ostringstream_type oss;
  oss << "Msg: " << msg << "\n"
      << "EVStack trace: \n"
      << stack_dump();
  return oss.str();
}

inline evaluated_value syntax_evaluator::evaluate_one(
    const evaluable_ptr& e, evaluation_context_ptr ctxt) {
  if (e) {
    stack_push(_details::get_name(e), std::move(ctxt));
    e->accept(this);
    auto frame = stack_pop();
    if (!frame.evaluated_vals.empty()) {
      return frame.evaluated_vals.back();
    }
  }
  return {};
}

inline string_type syntax_evaluator::dump(const evaluable_list_t& l) {
  ostringstream_type oss;
  oss << "( ";
  for (auto& e : l) {
    oss << _details::get_name(e);
  }
  oss << " )";
  return oss.str();
}

template <class _optype_t, _optype_t _optype_val,
          template <class> class _std_op,
          template <class, class> class _applier_impl>
evaluated_value syntax_evaluator::apply_op(const evaluated_values& frame) {
  auto first = frame.front();
  return std::visit(
      [&frame, this](auto&& val) {
        using val_type = __remove_cr_t<decltype(val)>;
        if constexpr (_details::operation_supported<val_type>(_optype_val)) {
          return _applier_impl<val_type, _std_op<val_type>>::apply(frame);
        } else {
          __throw_st<invalid_op_error>(str_join(
              "operation ", to_string(_optype_val),
              " must apply on all arguments of ", typeid(val).name(), " only"));
        }
        return evaluated_value{};
      },
      first.value);
}

template <class _operation, class _callable>
evaluated_value syntax_evaluator::evaluate_operation(const _operation& op,
                                                     _callable&& eval_func) {
  stack_push(_details::get_name(op));
  // evaluate all params
  for (auto& p : op.params) {
    p->accept(this);
  }
  // then apply the operation:
  auto e = eval_func(stack_top_frame().evaluated_vals);
  stack_pop();
  return e;
}

template <class _compare_method, size_t expected_count, class _operation>
void syntax_evaluator::validate_param_count(const _operation& o) {
  __throw_st_if<syntax_error>(
      _compare_method{}(o.params.size(), expected_count),
      str_join("Error: Invalid param count of `", to_string(o.type),
               "` Expected: ", expected_count, " - Real: ", o.params.size()));
}

template <class _operation>
void syntax_evaluator::make_sure_unary_op(const _operation& o) {
  validate_param_count<std::not_equal_to<>, 1>(o);
}

template <class T>
void syntax_evaluator::make_sure_binary_op(const __operator_base<T>& o) {
  validate_param_count<std::less<>, 2>(o);
}

template <class T>
void syntax_evaluator::make_sure_single_binary_op(const __operator_base<T>& o) {
  validate_param_count<std::not_equal_to<>, 2>(o);
}

template <class T, class _std_op>
struct apply_unary_op_impl {
  static evaluated_value apply(const evaluated_values& frame) {
    return evaluated_value{_std_op{}(frame.back().get<T>())};
  }
};

template <class T, class _std_op>
struct apply_single_bin_op_impl {
  static evaluated_value apply(const evaluated_values& frame) {
    return evaluated_value{
        _std_op{}(frame.front().get<T>(), frame.back().get<T>())};
  }
};

template <class T, class _std_op>
struct apply_muti_bin_op_impl {
  static evaluated_value apply(const evaluated_values& frame) {
    auto it = std::begin(frame);
    auto v = it->get<T>();
    _std_op applier;
    while (++it != std::end(frame)) {
      v = applier(v, it->get<T>());
    }
    return evaluated_value{v};
  }
};

template <class T, T _optype, template <class> class _std_op>
evaluated_value syntax_evaluator::apply_single_bin_op(
    const evaluated_values& frame) {
  return apply_op<T, _optype, _std_op, apply_single_bin_op_impl>(frame);
}

template <class T, T _optype, template <class> class _std_op>
evaluated_value syntax_evaluator::apply_multi_bin_op(
    const evaluated_values& frame) {
  return apply_op<T, _optype, _std_op, apply_muti_bin_op_impl>(frame);
}

template <class T, T _optype, template <class> class _std_op>
evaluated_value syntax_evaluator::apply_unary_op(
    const evaluated_values& frame) {
  return apply_op<T, _optype, _std_op, apply_unary_op_impl>(frame);
}

}  // namespace jas

#undef __MC_BASIC_OPERATION_EVAL_START
#undef __MC_BASIC_OPERATION_EVAL_END
