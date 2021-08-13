#include "SyntaxValidator.h"

#include <iomanip>
#include <string>

#include "EvaluableClasses.h"
#include "Exception.h"
#include "Keywords.h"

namespace jas {

class SyntaxValidatorImpl : public EvaluatorBase {
 public:
  SyntaxValidatorImpl() { os_ << std::boolalpha; }

  bool validate(const Evaluable& e) {
    e.accept(this);
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
    e.accept(this);
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

  void evaluate(const EvaluablePtr& e) { e->accept(this); }
  void evaluate(const Evaluable& e) { e.accept(this); }

  bool dumpstackVariables(const StackVariablesPtr& params) {
    if (params) {
      os_ << "set(";
      auto it = std::begin(*params);
      os_ << "$" << it->first << '=';
      this->evaluate(it->second.variable);
      for (; ++it != std::end(*params);) {
        os_ << JASSTR(",");
        os_ << "$" << it->first << '=';
        this->evaluate(it->second.variable);
      }
      os_ << ").then";
      return true;
    }
    return false;
  }
  void decorateId(const Evaluable& e) {
    if (!e.id.empty()) {
      os_ << prefix::variable << e.id << JASSTR("=");
    }
  }
  void eval(const DirectVal& val) override {
    os_ << JsonTrait::dump(val.value);
  }

  void eval(const EvaluableMap& e) override {
    dumpstackVariables(e.stackVariables);
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
    decorateId(op);
    if (op.type == ArithmeticOperatorType::bit_not ||
        op.type == ArithmeticOperatorType::negate) {
      dumpPreUnaryOp(op);
    } else {
      dumpBinaryOp(op);
    }
  }

  void eval(const LogicalOperator& op) override {
    decorateId(op);
    if (op.type == LogicalOperatorType::logical_not) {
      dumpPreUnaryOp(op);
    } else {
      dumpBinaryOp(op);
    }
  }
  void eval(const ComparisonOperator& op) override {
    decorateId(op);
    dumpBinaryOp(op, true);
  }

  void eval(const ListOperation& op) override {
    dumpstackVariables(op.stackVariables);
    decorateId(op);
    os_ << op.type << JASSTR("(");
    if (!op.list) {
      os_ << std::quoted(JASSTR("current_list"));
    } else {
      op.list->accept(this);
    }
    if (op.type != ListOperationType::transform) {
      os_ << JASSTR(").satisfies(");
    } else {
      os_ << JASSTR(").with(");
    }
    op.cond->accept(this);

    os_ << JASSTR(")");
  }

  void eval(const Function& fnc) override {
    dumpstackVariables(fnc.stackVariables);
    decorateId(fnc);
    os_ << (fnc.name.empty()
                ? bookMarkError(JASSTR("Funtion name must not be empty"))
                : fnc.name);
    os_ << JASSTR("(");

    if (fnc.param) {
      fnc.param->accept(this);
    }
    os_ << JASSTR(")");
  }

  void eval(const VariableFieldQuery& vfq) override {
    os_ << vfq.id << "[";
    if (!vfq.field_path.empty()) {
      auto it = std::begin(vfq.field_path);
      (*it)->accept(this);
      while (++it != std::end(vfq.field_path)) {
        os_ << '/';
        (*it)->accept(this);
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

  String bookMarkError(const String& err) {
    constexpr CharType error_indicator[] = JASSTR("^^");
    errors_.emplace_back(
        (std::streamsize)os_.tellp() + sizeof(error_indicator) / 2 + 1, err);
    return error_indicator;
  }

  template <class T>
  void dumpPreUnaryOp(const _OperatorBase<T, typename T::OperatorType>& op) {
    dumpstackVariables(op.stackVariables);
    os_ << JASSTR("(");
    os_ << op.type;
    if (op.params.empty()) {
      os_ << bookMarkError(JASSTR("Missing paramter"));
    } else {
      op.params[0]->accept(this);
      if (op.params.size() > 1) {
        os_ << bookMarkError(JASSTR("More than required parameters count"));
      }
    }
    os_ << JASSTR(")");
  }

  template <class T>
  void dumpBinaryOp(const _OperatorBase<T, typename T::OperatorType>& op,
                    bool exact2 = false) {
    dumpstackVariables(op.stackVariables);
    os_ << JASSTR("(");
    if (op.params.size() < 2) {
      if (op.params.empty()) {
        os_ << bookMarkError(JASSTR("Parameter is missing")) << op.type
            << bookMarkError(JASSTR("Parameter is missing"));
      } else {
        op.params[0]->accept(this);
        os_ << JASSTR(" ") << op.type
            << bookMarkError(JASSTR("Parameter is missing"));
      }
    } else {
      auto it = std::begin(op.params);
      (*it)->accept(this);
      os_ << JASSTR(' ') << op.type << JASSTR(' ');
      (*(++it))->accept(this);
      if (!exact2) {
        while (++it != std::end(op.params)) {
          os_ << JASSTR(' ') << op.type << JASSTR(' ');
          (*it)->accept(this);
        }
      } else {
        while (++it != std::end(op.params)) {
          os_ << JASSTR(' ') << op.type << JASSTR(' ')
              << bookMarkError(JASSTR("More than required parameters count"));
          (*it)->accept(this);
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
