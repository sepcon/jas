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
    os_.str("");
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
      return "invalid";
    }
  }

  String getReport() { return os_.str(); }

  bool hasError() const { return !errors_.empty(); }

  void flushAllErrors() {
    auto summaryErrStart = [this] {
      os_ << "\n>"
             "\n>"
             "\n>> ERRORS:"
          << errors_.size();
    };
    auto summaryErrEnd = [this] {
      os_ << "\n>"
             "\n>";
    };

    if (!errors_.empty()) {
      int i = 0;
      std::streamsize prevPos = 0;

      os_ << "\n";
      for (auto& [pos, errmsg] : errors_) {
        os_ << std::setfill(' ') << std::setw(pos - prevPos) << "^(" << i++
            << ")";
        prevPos = pos + 2;
      }

      i = 0;

      summaryErrStart();
      for (auto& [pos, errmsg] : errors_) {
        os_ << "\n>> (" << i++ << "): " << errmsg;
      }
      summaryErrEnd();
    } else {
      summaryErrStart();
      summaryErrEnd();
    }
  }

  void evaluate(const EvaluablePtr& e) { e->accept(this); }
  void evaluate(const Evaluable& e) { e.accept(this); }
  void decorateId(const Evaluable& e) {
    if (!e.id.empty()) {
      os_ << prefix::property << e.id << "=";
    }
  }
  void eval(const DirectVal& val) override {
    os_ << JsonTrait::dump(val.value);
  }

  void eval(const EvaluableMap& e) override {
    os_ << "{";
    if (!e.value.empty()) {
      auto it = std::begin(e.value);
      auto dumpkv = [this](auto&& k, auto&& v) {
        os_ << std::quoted(k) << ":";
        this->evaluate(v);
      };

      dumpkv(it->first, it->second);

      while (++it != std::end(e.value)) {
        os_ << ",";
        dumpkv(it->first, it->second);
      }
    }
    os_ << "}";
  }

  void eval(const EvaluableArray& e) override {
    os_ << "[";
    auto it = std::begin(e.value);
    evaluate(*it);
    while (++it != std::end(e.value)) {
      os_ << ",";
      evaluate(*it);
    }
    os_ << "]";
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
    decorateId(op);
    os_ << op.type << "(";

    if (!op.list) {
      os_ << std::quoted("current_list");
    } else {
      op.list->accept(this);
    }
    os_ << ").satisfies(";

    op.cond->accept(this);

    os_ << ")";
  }

  void eval(const Function& fnc) override {
    decorateId(fnc);
    os_ << (fnc.name.empty() ? bookMarkError("Funtion name must not be empty")
                             : fnc.name);
    os_ << "(";
    if (fnc.param) {
      fnc.param->accept(this);
    }
    os_ << ")";
  }

  void eval(const ContextVal& ctv) override {
    decorateId(ctv);
    os_ << "value_of(" << std::quoted(ctv.path);
    switch (ctv.snapshot) {
      case ContextVal::new_:
        break;
      case ContextVal::old:
        os_ << ", snapshot=old";
        break;
      default: {
        os_ << bookMarkError("Wrong snapshot index value");
        os_ << "invalid(" << ctv.snapshot << ")";
      } break;
    }
    os_ << ")";
  }

  void eval(const Property& rv) override {
    os_ << "property(";
    if (rv.id.empty()) {
      os_ << bookMarkError("a `Property Name` must not be empty");
    } else {
      os_ << std::quoted(rv.id);
    }
    os_ << ")";
  }

  String bookMarkError(const String& err) {
    constexpr char error_indicator[] = "^^";
    errors_.emplace_back(
        (std::streamsize)os_.tellp() + sizeof(error_indicator) / 2 + 1, err);
    return error_indicator;
  }

  template <class T>
  void dumpPreUnaryOp(const _OperatorBase<T, typename T::OperatorType>& op) {
    os_ << "(";
    os_ << op.type;
    if (op.params.empty()) {
      os_ << bookMarkError("Missing paramter");
    } else {
      op.params[0]->accept(this);
      if (op.params.size() > 1) {
        os_ << bookMarkError("More than required parameters count");
      }
    }
    os_ << ")";
  }

  template <class T>
  void dumpBinaryOp(const _OperatorBase<T, typename T::OperatorType>& op,
                    bool exact2 = false) {
    os_ << "(";
    if (op.params.size() < 2) {
      if (op.params.empty()) {
        os_ << bookMarkError("Parameter is missing") << op.type
            << bookMarkError("Parameter is missing");
      } else {
        op.params[0]->accept(this);
        os_ << " " << op.type << bookMarkError("Parameter is missing");
      }
    } else {
      auto it = std::begin(op.params);
      (*it)->accept(this);
      os_ << ' ' << op.type << ' ';
      (*(++it))->accept(this);
      if (!exact2) {
        while (++it != std::end(op.params)) {
          os_ << ' ' << op.type << ' ';
          (*it)->accept(this);
        }
      } else {
        while (++it != std::end(op.params)) {
          os_ << ' ' << op.type << ' '
              << bookMarkError("More than required parameters count");
          (*it)->accept(this);
        }
      }
    }
    os_ << ")";
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
String SyntaxValidator::generate_syntax(const Evaluable& e) {
  return impl_->generateSyntax(e);
}
String SyntaxValidator::generate_syntax(const EvaluablePtr& e) {
  return impl_->generateSyntax(e);
}
String SyntaxValidator::get_report() { return impl_->getReport(); }
bool SyntaxValidator::has_error() const { return impl_->hasError(); }
}  // namespace jas
