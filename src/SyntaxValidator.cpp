#include "jas/SyntaxValidator.h"

#include <iomanip>
#include <string>

#include "jas/EvaluableClasses.h"
#include "jas/Exception.h"
#include "jas/Keywords.h"

namespace jas {

class SyntaxValidatorImpl : public EvaluatorBase {
 public:
  SyntaxValidatorImpl() { os_ << std::boolalpha; }

  bool validate(const Evaluable& e) {
    _eval(&e);
    flushAllErrors();
    return errors_.empty();
  }
  bool validate(const EvaluablePtr& e) {
    if (e) {
      return validate(*e);
    } else {
      return false;
    }
  }

  void clear() {
    os_.str(JASSTR(""));
    errors_.clear();
  }

  String generateSyntax(const Evaluable& e) {
    _eval(&e);
    return os_.str();
  }

  String generateSyntax(const EvaluablePtr& e) {
    if (e) {
      return generateSyntax(*e);
    } else {
      return JASSTR("invalid");
    }
  }

  String getReport() { return JASSTR("\n") + os_.str(); }

  bool hasError() const { return !errors_.empty(); }

  void flushAllErrors() {
    auto summaryErrStart = [this] {
      os_ << JASSTR("\n>--\n>> ERRORS:") << errors_.size();
    };
    auto summaryErrEnd = [this] { os_ << JASSTR("\n>--"); };

    if (!errors_.empty()) {
      int i = 0;
      std::streamsize prevPos = 0;

      os_ << JASSTR("\n");
      for (auto& [pos, errmsg] : errors_) {
        os_ << std::setfill(JASSTR(' ')) << std::setw(pos - prevPos)
            << JASSTR("^(") << i++ << JASSTR(")");
        prevPos = pos + 2;
      }

      i = 0;

      summaryErrStart();
      for (auto& [pos, errmsg] : errors_) {
        os_ << JASSTR("\n>> (") << i++ << JASSTR("): ") << errmsg;
      }
      summaryErrEnd();
    } else {
      summaryErrStart();
      summaryErrEnd();
    }
  }

  void evaluate(const EvaluablePtr& e) { _eval(e.get()); }
  void evaluate(const Evaluable& e) { _eval(&e); }

  bool dumpstackVariables(const StackVariablesPtr& params) {
    if (params) {
      os_ << "set(";
      auto it = std::begin(*params);
      os_ << "$" << it->name << '=';
      this->evaluate(it->variable);
      for (; ++it != std::end(*params);) {
        os_ << JASSTR(",");
        os_ << "$" << it->name << '=';
        this->evaluate(it->variable);
      }
      os_ << ").then";
      return true;
    }
    return false;
  }
  bool decorateId(const Evaluable& e) {
    if (!e.id.empty()) {
      os_ << prefix::variable << e.id << JASSTR("=");
      return true;
    }
    return false;
  }

  bool _evalStackEvb(const __UseStackEvaluable& evb) {
    auto ret = decorateId(evb);
    ret |= dumpstackVariables(evb.stackVariables);
    return ret;
  }

  void eval(const DirectVal& val) override {
    os_ << JsonTrait::dump(val.value);
  }

  void eval(const EvaluableMap& e) override {
    os_ << JASSTR("{");
    if (!e.value.empty()) {
      auto it = std::begin(e.value);
      auto dumpkv = [this](auto&& k, auto&& v) {
        os_ << std::quoted(k) << JASSTR(":");
        this->evaluate(v);
      };

      dumpkv(it->first, it->second);

      while (++it != std::end(e.value)) {
        os_ << JASSTR(",");
        dumpkv(it->first, it->second);
      }
    }
    os_ << JASSTR("}");
  }

  void eval(const EvaluableArray& e) override {
    os_ << JASSTR("[");
    if (!e.value.empty()) {
      auto it = std::begin(e.value);
      evaluate(*it);
      while (++it != std::end(e.value)) {
        os_ << JASSTR(",");
        evaluate(*it);
      }
    }
    os_ << JASSTR("]");
  }

  void eval(const ArithmaticalOperator& op) override {
    if (op.type == aot::bit_not || op.type == aot::negate) {
      dumpPreUnaryOp(op);
    } else {
      dumpBinaryOp(op);
    }
  }
  void eval(const ArthmSelfAssignOperator& op) override {
    os_ << JASSTR("(");
    if (op.params.size() >= 1) {
      if (!isType<Variable>(op.params.front())) {
        os_ << bookMarkError(
            strJoin("First argument of `", op.type, "` must be variable"));
      } else {
        os_ << prefix::variable << op.params.front()->id << op.type;
      }

      if (op.params.size() >= 2) {
        _eval(op.params[1].get());
        if (op.params.size() > 2) {
          os_ << bookMarkError(
              strJoin("Redundant argument to operator `", op.type, "`"));
          _eval(op.params[2].get());
          os_ << "...";
        }
      } else {
        os_ << bookMarkError(
            strJoin("Missing argument to operator `", op.type, "`"));
      }
    } else {
      os_ << bookMarkError(JASSTR("Missing variable name")) << op.type
          << bookMarkError(
                 strJoin("Missing argument to operator `", op.type, "`"));
    }
    os_ << JASSTR(")");
  }

  void eval(const LogicalOperator& op) override {
    if (op.type == lot::logical_not) {
      dumpPreUnaryOp(op);
    } else {
      dumpBinaryOp(op);
    }
  }
  void eval(const ComparisonOperator& op) override { dumpBinaryOp(op, true); }

  void eval(const ListOperation& op) override {
    os_ << op.type << JASSTR("(");
    if (!op.list) {
      os_ << std::quoted(JASSTR("current_list"));
    } else {
      _eval(op.list.get());
    }
    if (op.type != lsot::transform) {
      os_ << JASSTR(").satisfies(");
    } else {
      os_ << JASSTR(").with(");
    }
    _eval(op.cond.get());

    os_ << JASSTR(")");
  }

  void eval(const FunctionInvocation& fnc) override {
    os_ << fnc.moduleName << (fnc.moduleName.empty() ? "" : ".")
        << (fnc.name.empty()
                ? bookMarkError(JASSTR("Funtion name must not be empty"))
                : fnc.name);
    os_ << JASSTR("(");

    _eval(fnc.param.get());
    os_ << JASSTR(")");
  }

  void eval(const VariableFieldQuery& vfq) override {
    os_ << vfq.id << "[";
    if (!vfq.field_path.empty()) {
      auto it = std::begin(vfq.field_path);
      _eval(it->get());
      while (++it != std::end(vfq.field_path)) {
        os_ << '/';
        _eval(it->get());
      }
    } else {
      os_ << bookMarkError(
          strJoin("No key or path for accessing variable `", vfq.id, "`"));
    }
    os_ << "]";
  }

  void eval(const Variable& rv) override {
    os_ << prefix::variable;
    if (rv.id.empty()) {
      os_ << bookMarkError(JASSTR("a `Property Name` must not be empty"));
    } else {
      os_ << rv.id;
    }
  }

  void _eval(const Evaluable* evb) {
    if (evb) {
      if (evb->useStack()) {
        auto shouldClose = false;
        if (_evalStackEvb(static_cast<const __UseStackEvaluable&>(*evb))) {
          shouldClose = true;
          os_ << "(";
        }
        evb->accept(this);
        if (shouldClose) {
          os_ << ")";
        }
      } else {
        evb->accept(this);
      }
    }
  }

  String bookMarkError(const String& err) {
    constexpr CharType error_indicator[] = JASSTR("??");
    errors_.emplace_back(
        (std::streamsize)os_.tellp() + sizeof(error_indicator) / 2 + 1, err);
    return error_indicator;
  }

  template <class T>
  void dumpPreUnaryOp(const _OperatorBase<T, typename T::OperatorType>& op) {
    os_ << JASSTR("(");
    os_ << op.type;
    if (op.params.empty()) {
      os_ << bookMarkError(JASSTR("Missing paramter"));
    } else {
      _eval(op.params[0].get());
      if (op.params.size() > 1) {
        os_ << bookMarkError(JASSTR("More than required parameters count"));
      }
    }
    os_ << JASSTR(")");
  }

  template <class T>
  void dumpBinaryOp(const _OperatorBase<T, typename T::OperatorType>& op,
                    bool exact2 = false) {
    os_ << JASSTR("(");
    if (op.params.size() < 2) {
      if (op.params.empty()) {
        os_ << bookMarkError(JASSTR("Parameter is missing")) << op.type
            << bookMarkError(JASSTR("Parameter is missing"));
      } else {
        _eval(op.params[0].get());
        os_ << JASSTR(" ") << op.type
            << bookMarkError(JASSTR("Parameter is missing"));
      }
    } else {
      auto it = std::begin(op.params);
      _eval((*it).get());
      os_ << JASSTR(' ') << op.type << JASSTR(' ');
      _eval((*(++it)).get());
      if (!exact2) {
        while (++it != std::end(op.params)) {
          os_ << JASSTR(' ') << op.type << JASSTR(' ');
          _eval((*it).get());
        }
      } else {
        while (++it != std::end(op.params)) {
          os_ << JASSTR(' ') << op.type << JASSTR(' ')
              << bookMarkError(JASSTR("More than required parameters count"));
          _eval((*it).get());
        }
      }
    }
    os_ << JASSTR(")");
  }

  OStringStream os_;
  std::vector<std::pair<std::streamsize, String>> errors_;
};

SyntaxValidator::SyntaxValidator() : impl_{new SyntaxValidatorImpl} {}

SyntaxValidator::~SyntaxValidator() { delete impl_; }

bool SyntaxValidator::validate(const Evaluable& e) {
  return impl_->validate(e);
}
bool SyntaxValidator::validate(const EvaluablePtr& e) {
  return impl_->validate(e);
}
void SyntaxValidator::clear() { return impl_->clear(); }
String SyntaxValidator::generateSyntax(const Evaluable& e) {
  return impl_->generateSyntax(e);
}
String SyntaxValidator::generateSyntax(const EvaluablePtr& e) {
  return impl_->generateSyntax(e);
}
String SyntaxValidator::getReport() { return impl_->getReport(); }
bool SyntaxValidator::hasError() const { return impl_->hasError(); }
}  // namespace jas
