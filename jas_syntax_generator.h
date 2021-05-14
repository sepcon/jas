#pragma once

#include <iomanip>
#include <ostream>
#include <string>

#include "jas_evaluables.h"
#include "jas_exception.h"

namespace jas {

class syntax_generator : public evaluator_base {
 public:
  syntax_generator(ostream_type& os) : os_(os) { os << std::boolalpha; }

  void flush_all_errors() {
    int i = 0;
    std::streamsize prev_pos = 0;

    os_ << "\n";
    for (auto& [pos, errmsg] : errors_) {
      os_ << std::setfill(' ') << std::setw(pos - prev_pos) << "^(" << i++
          << ")";
      prev_pos = pos + i;
    }

    i = 0;
    os_ << "\n-------------------------"
           "\nERRORS:";
    for (auto& [pos, errmsg] : errors_) {
      os_ << "\n(" << i++ << "): " << errmsg;
    }
    os_ << "\n-------------------------\n";
  }
  void eval(const constant_val& val) override {
    std::visit(
        [this](const auto& v) {
          if constexpr (std::is_same_v<string_type,
                                       __remove_cr_t<decltype(v)>>) {
            os_ << std::quoted(v);
          } else {
            os_ << v;
          }
        },
        val.value);
  }

  void eval(const arithmatic_operation& op) override {
    if (op.type == arithmetic_operator_type::bit_not ||
        op.type == arithmetic_operator_type::negate) {
      dump_pre_unary_op(op);
    } else {
      dump_binary_op(op);
    }
  }

  void eval(const logical_operation& op) override {
    if (op.type == logical_operation_type::logical_not) {
      dump_pre_unary_op(op);
    } else {
      dump_binary_op(op);
    }
  }
  void eval(const evb_comparison_op& op) override { dump_binary_op(op, true); }

  void eval(const list_operation& a) override {
    os_ << to_string(a.type) << "(" << std::quoted(a.list_path)
        << ").satisfies(";
    a.cond->accept(this);
    os_ << ")";
  }

  void eval(const function_invoker& fi) override {
    os_ << "call("
        << (fi.func_name.empty()
                ? syntax_error_string("Funtion name must not be empty")
                : fi.func_name);
    if (fi.param) {
      os_ << "(";
      fi.param->accept(this);
      os_ << ")";
    }
    os_ << ")";
  }

  void eval(const context_value_collector& fc) override {
    os_ << "value_of(" << std::quoted(fc.value_path);
    if (fc.snapshot_idx == context_value_collector::old_snapshot_idx) {
      os_ << ", snapshot=old";
    }
    os_ << ")";
  }

 private:
  string_type syntax_error_string(const string_type& err) {
    errors_.emplace_back((std::streamsize)os_.tellp() + 3, err);
    return " _^_ ";
  }

  template <class T>
  void dump_pre_unary_op(const __operator_base<T>& op) {
    os_ << "(";
    os_ << to_string(op.type);
    if (op.params.empty()) {
      os_ << syntax_error_string("Missing paramter");
    } else {
      op.params[0]->accept(this);
      if (op.params.size() > 1) {
        os_ << syntax_error_string("More than required parameters count");
      }
    }
    os_ << ")";
  }

  template <class T>
  void dump_binary_op(const __operator_base<T>& op, bool exact2 = false) {
    auto s_op = to_string(op.type);
    os_ << "(";
    if (op.params.size() < 2) {
      auto invalid_param_s = syntax_error_string("Parameter is missing");
      if (op.params.empty()) {
        os_ << invalid_param_s << s_op << invalid_param_s;
      } else {
        op.params[0]->accept(this);
        os_ << " " << s_op << invalid_param_s;
      }
    } else {
      auto it = std::begin(op.params);
      (*it)->accept(this);
      os_ << ' ' << s_op << ' ';
      (*(++it))->accept(this);
      if (!exact2) {
        while (++it != std::end(op.params)) {
          os_ << ' ' << s_op << ' ';
          (*it)->accept(this);
        }
      } else {
        while (++it != std::end(op.params)) {
          os_ << ' ' << s_op << ' '
              << syntax_error_string("More than required parameters count");
          (*it)->accept(this);
        }
      }
    }
    os_ << ")";
  }

  ostream_type& os_;
  std::vector<std::pair<std::streamsize, string_type>> errors_;
};

namespace sytax_dump {
inline ostream_type& operator<<(ostream_type& os, const evaluable_base& e) {
  syntax_generator g(os);
  e.accept(&g);
  g.flush_all_errors();
  return os;
}

}  // namespace sytax_dump
}  // namespace jas
