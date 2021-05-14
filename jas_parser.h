#pragma once

#include <map>
#include <optional>
#include <vector>

#include "jas_evaluables.h"

namespace jas {

namespace keyword {

static constexpr auto bit_and = "@bit_and";
static constexpr auto bit_not = "@bit_not";
static constexpr auto bit_or = "@bit_or";
static constexpr auto bit_xor = "@bit_xor";
static constexpr auto divides = "@divides";
static constexpr auto minus = "@minus";
static constexpr auto modulus = "@modulus";
static constexpr auto multiplies = "@multiplies";
static constexpr auto negate = "@negate";
static constexpr auto plus = "@plus";
// static constexpr auto  =
static constexpr auto logical_and = "@and";
static constexpr auto logical_not = "@not";
static constexpr auto logical_or = "@or";
// static constexpr auto  =
static constexpr auto eq = "@eq";
static constexpr auto ge = "@ge";
static constexpr auto gt = "@gt";
static constexpr auto le = "@le";
static constexpr auto lt = "@lt";
static constexpr auto neq = "@neq";
// static constexpr auto  =
static constexpr auto all_of = "@all_of";
static constexpr auto any_of = "@any_of";
static constexpr auto none_of = "@none_of";
static constexpr auto size_of = "@size_of";

static constexpr auto cond = "@cond";
static constexpr auto item_id = "@item_id";
static constexpr auto list_path = "@list_path";

// static constexpr auto  =
static constexpr auto func = "@func";
static constexpr auto key = "@key";
static constexpr auto snapshot = "@snapshot";
static constexpr auto name = "@name";
static constexpr auto param = "@param";
}  // namespace keyword

template <class _json_trait>
class parser {
 public:
  using jtrait_type = _json_trait;
  using json_type = typename jtrait_type::json_type;
  using jobject_type = typename jtrait_type::object_type;
  using jarray_type = typename jtrait_type::array_type;
  using jstring_type = typename jtrait_type::string_type;
  using parse_rule_callback = std::function<evaluable_ptr(const json_type&)>;
  using keyword_parser_map = std::map<jstring_type, parse_rule_callback>;

  static evaluable_ptr parse(const json_type& j) { return generic_parse(j); }

 private:
  struct invalid_parser {
    evaluable_ptr operator()(const json_type&) { return {}; }
  };

  struct constant_val_parser {
    evaluable_ptr operator()(const json_type& j) {
      if (jtrait_type::is_int(j)) {
        return make_const(jtrait_type::template get<int64_t>(j));
      } else if (jtrait_type::is_double(j)) {
        return make_const(jtrait_type::template get<double>(j));
      } else if (jtrait_type::is_bool(j)) {
        return make_const(jtrait_type::template get<bool>(j));
      } else if (jtrait_type::is_string(j)) {
        return make_const(jtrait_type::template get<string_type>(j));
      } else {
        return {};
      }
    }
  };

  static std::optional<std::pair<string_type, json_type>> extract_op_params(
      const json_type& j) {
    if (jtrait_type::is_object(j) && jtrait_type::size(j) == 1) {
      auto oj = jtrait_type::template get<jobject_type>(j);
      for (auto& [key, val] : oj) {
        return std::make_pair(key, val);
      }
    }
    return {};
  }

  template <class _op_parser, class _operation>
  struct operator_parser_base {
    using operation_type = typename _operation::operator_t;
    static operation_type _to_operator(const jstring_type& sop) {
      if (auto it = _op_parser::s2op().find(sop);
          it != std::end(_op_parser::s2op())) {
        return it->second;
      } else {
        return operation_type::invalid;
      }
    }

    evaluable_ptr operator()(const json_type& j) {
      if (auto extracted = extract_op_params(j)) {
        auto& [sop, jevaluation] = extracted.value();
        if (auto op = _to_operator(sop); op != operation_type::invalid) {
          if (jtrait_type::is_array(jevaluation)) {
            evaluable_list_t params;
            for (auto& jparam :
                 jtrait_type::template get<jarray_type>(jevaluation)) {
              params.emplace_back(parser::generic_parse(jparam));
            }
            return make_op(op, std::move(params));
          }
        }
      }
      return {};
    }
  };

  struct arthm_op_parser
      : public operator_parser_base<arthm_op_parser, arithmatic_operation> {
    static const std::map<jstring_type, arithmetic_operator_type>& s2op() {
      static std::map<jstring_type, arithmetic_operator_type> _ = {
          {keyword::plus, arithmetic_operator_type::plus},
          {keyword::minus, arithmetic_operator_type::minus},
          {keyword::multiplies, arithmetic_operator_type::multiplies},
          {keyword::divides, arithmetic_operator_type::divides},
          {keyword::modulus, arithmetic_operator_type::modulus},
          {keyword::bit_and, arithmetic_operator_type::bit_and},
          {keyword::bit_or, arithmetic_operator_type::bit_or},
          {keyword::bit_not, arithmetic_operator_type::bit_not},
          {keyword::bit_xor, arithmetic_operator_type::bit_xor},
          {keyword::negate, arithmetic_operator_type::negate},
      };
      return _;
    }
  };
  struct logical_op_parser
      : public operator_parser_base<logical_op_parser, logical_operation> {
    static const std::map<jstring_type, logical_operation_type>& s2op() {
      static std::map<jstring_type, logical_operation_type> _ = {
          {keyword::logical_and, logical_operation_type::logical_and},
          {keyword::logical_or, logical_operation_type::logical_or},
          {keyword::logical_not, logical_operation_type::logical_not},
      };
      return _;
    }
  };
  struct cmp_op_parser
      : public operator_parser_base<cmp_op_parser, evb_comparison_op> {
    static const std::map<jstring_type, comparison_operator_type>& s2op() {
      static std::map<jstring_type, comparison_operator_type> _ = {
          {keyword::eq, comparison_operator_type::eq},
          {keyword::neq, comparison_operator_type::neq},
          {keyword::lt, comparison_operator_type::lt},
          {keyword::gt, comparison_operator_type::gt},
          {keyword::le, comparison_operator_type::le},
          {keyword::ge, comparison_operator_type::ge},
      };
      return _;
    }
  };

  struct list_op_parser {
    static list_operation_type _to_operator(const jstring_type& sop) {
      static std::map<jstring_type, list_operation_type> s2op = {
          {keyword::any_of, list_operation_type::any_of},
          {keyword::all_of, list_operation_type::all_of},
          {keyword::none_of, list_operation_type::none_of},
          {keyword::size_of, list_operation_type::size_of},
      };
      if (auto it = s2op.find(sop); it != std::end(s2op)) {
        return it->second;
      } else {
        return list_operation_type::invalid;
      }
    }

    evaluable_ptr operator()(const json_type& j) {
      auto extracted = extract_op_params(j);
      if (extracted) {
        auto& [sop, jevaluable] = extracted.value();
        if (auto op = _to_operator(sop); op != list_operation_type::invalid) {
          if (jtrait_type::is_object(jevaluable)) {
            if (auto cond = parser::generic_parse(
                    jtrait_type::template get(jevaluable, keyword::cond))) {
              return make_op(op, std::move(cond),
                             jtrait_type::template get<string_type>(
                                 jevaluable, keyword::item_id),
                             jtrait_type::template get<string_type>(
                                 jevaluable, keyword::list_path));
            }
          }
        }
      }
      return {};
    }
  };

  struct func_invoker_parser {
    evaluable_ptr operator()(const json_type& j) {
      auto extracted = extract_op_params(j);
      if (extracted) {
        auto& [sop, jevaluable] = extracted.value();
        if (sop == keyword::func) {
          if (jtrait_type::is_string(jevaluable)) {
            return make_func(
                jtrait_type::template get<string_type>(jevaluable));
          } else if (jtrait_type::is_object(jevaluable)) {
            return make_func(jtrait_type::template get<string_type>(
                                 jevaluable, keyword::name),
                             parser::generic_parse(jtrait_type::template get(
                                 jevaluable, keyword::param)));
          }
        }
      }
      return {};
    }
  };

  struct context_value_collector_parser {
    evaluable_ptr operator()(const json_type& j) {
      auto extracted = extract_op_params(j);
      if (extracted) {
        auto& [sop, jevaluable] = extracted.value();
        if (sop != keyword::key) {
          return {};
        }
        if (jtrait_type::is_string(jevaluable)) {
          return make_ctxt_value_collector(
              jtrait_type::template get<string_type>(jevaluable));
        } else if (jtrait_type::is_object(jevaluable)) {
          auto keyname =
              jtrait_type::template get<string_type>(jevaluable, keyword::name);
          auto jsnapshot = jtrait_type::get(jevaluable, keyword::snapshot);
          int32_t snapshot_idx = context_value_collector::new_snapshot_idx;
          if (jtrait_type::is_int(jsnapshot)) {
            snapshot_idx = jtrait_type::template get<int32_t>(jsnapshot);
          }
          return make_ctxt_value_collector(std::move(keyname), snapshot_idx);
        }
      }
      return {};
    }
  };

  static evaluable_ptr generic_parse(const json_type& j) {
    for (auto& parser : parsers()) {
      if (auto evaled = parser(j)) {
        return evaled;
      }
    }
    return {};
  }

  static std::vector<parse_rule_callback>& parsers() {
    static std::vector<parse_rule_callback> _ = {
        arthm_op_parser{},     logical_op_parser{},
        cmp_op_parser{},       list_op_parser{},
        func_invoker_parser{}, context_value_collector_parser{},
        constant_val_parser{}, invalid_parser{}};
    return _;
  }

  static const parse_rule_callback& get_parser(const jstring_type& keyword) {
    static keyword_parser_map parsers_map_ = {
        // Arithmetic operations
        {keyword::plus, arthm_op_parser{}},
        {keyword::minus, arthm_op_parser{}},
        {keyword::multiplies, arthm_op_parser{}},
        {keyword::divides, arthm_op_parser{}},
        {keyword::modulus, arthm_op_parser{}},
        {keyword::bit_and, arthm_op_parser{}},
        {keyword::bit_or, arthm_op_parser{}},
        {keyword::bit_not, arthm_op_parser{}},
        {keyword::bit_xor, arthm_op_parser{}},
        {keyword::negate, arthm_op_parser{}},

        // Logical operations
        {keyword::logical_and, logical_op_parser{}},
        {keyword::logical_or, logical_op_parser{}},
        {keyword::logical_not, logical_op_parser{}},

        // Comparison operations
        {keyword::eq, cmp_op_parser{}},
        {keyword::neq, cmp_op_parser{}},
        {keyword::lt, cmp_op_parser{}},
        {keyword::gt, cmp_op_parser{}},
        {keyword::le, cmp_op_parser{}},
        {keyword::ge, cmp_op_parser{}},

        // list operations
        {keyword::any_of, list_op_parser{}},
        {keyword::all_of, list_op_parser{}},
        {keyword::none_of, list_op_parser{}},
        {keyword::size_of, list_op_parser{}},

        // others
        {keyword::func, func_invoker_parser{}},
        {keyword::key, context_value_collector_parser{}}};

    if (auto it = parsers_map_.find(keyword); it != std::end(parsers_map_)) {
      return it->second;
    } else {
      return invalid_parser{};
    }
  }
};
}  // namespace jas
