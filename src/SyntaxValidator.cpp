#include "jas/SyntaxValidator.h"

#include <iomanip>
#include <string>

#include "jas/EvaluableClasses.h"
#include "jas/Exception.h"
#include "jas/Keywords.h"

namespace jas {

class SyntaxValidatorImpl : public EvaluatorIF {
 public:
  SyntaxValidatorImpl() { os_ << std::boolalpha; }

  bool validate(const Evaluable& e) {
    clear();
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
    clear();
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

  bool _dumpstackVariables(const LocalVariablesPtr& params) {
    if (params) {
      os_ << "set(";
      auto it = std::begin(*params);
      os_ << "$" << it->first << '=';
      this->evaluate(it->second.value);
      for (; ++it != std::end(*params);) {
        os_ << JASSTR(",");
        os_ << "$" << it->first << '=';
        this->evaluate(it->second.value);
      }
      os_ << ").then";
      return true;
    }
    return false;
  }

  bool _evalStackSymbols(const UseStackEvaluable& evb) {
    return _dumpstackVariables(evb.localVariables);
  }

  void eval(const Constant& val) override { os_ << val.value.dump(); }

  void eval(const EvaluableDict& e) override {
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

  void eval(const EvaluableList& e) override {
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
        evaluate(op.params.front());
        os_ << bookMarkError(
            strJoin("First argument of `", op.type, "` must be variable"));
      } else {
        os_ << prefix::variable
            << static_cast<const Variable*>(op.params.front().get())->name;
      }
      os_ << op.type;
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

  void eval(const ListAlgorithm& op) override {
    os_ << op.type << JASSTR("(");
    if (!op.list) {
      os_ << std::quoted(JASSTR("current_list"));
    } else {
      _eval(op.list.get());
    }
    if (op.type != lsaot::transform) {
      os_ << JASSTR(").satisfies(");
    } else {
      os_ << JASSTR(").with(");
    }
    _eval(op.cond.get());

    os_ << JASSTR(")");
  }

  template <class _FI>
  void _eval(const FunctionInvocationBase<_FI>& fnc) {
    os_ << (fnc.name.empty()
                ? bookMarkError(JASSTR("Funtion name must not be empty"))
                : fnc.name);
    os_ << JASSTR("(");

    _eval(fnc.param.get());
    os_ << JASSTR(")");
  }

  void eval(const EvaluatorFI& fnc) override { _eval<EvaluatorFI>(fnc); }
  void eval(const ContextFI& fnc) override { _eval<ContextFI>(fnc); }
  void eval(const ModuleFI& fnc) override {
    auto moduleName = fnc.module ? fnc.module->moduleName() : String{};
    os_ << moduleName << (moduleName.empty() ? "" : ".");
    _eval<ModuleFI>(fnc);
  }

  void eval(const MacroFI& macro) override {
    os_ << (macro.name.empty()
                ? bookMarkError(JASSTR("Funtion name must not be empty"))
                : macro.name);
    os_ << JASSTR("(");
    if (macro.param) {
      if (isType<EvaluableList>(macro.param)) {
        auto listParams = static_cast<const EvaluableList*>(macro.param.get());
        assert(!listParams->value.empty() &&
               "List params must not be empty at this point");
        auto it = std::begin(listParams->value);
        _eval(it->get());
        while (++it != std::end(listParams->value)) {
          os_ << JASSTR(",");
          _eval(it->get());
        }
      } else if (isType<Constant>(macro.param)) {
        auto asConst = static_cast<const Constant*>(macro.param.get());
        assert(asConst->value.isList() && "Must be a list here");
        auto it = std::begin(asConst->value.asList());
        os_ << *it;
        while (++it != std::end(asConst->value.asList())) {
          os_ << JASSTR(",");
          os_ << *it;
        }
      }
    }
    os_ << JASSTR(")");
  }

  void eval(const ObjectPropertyQuery& vfq) override {
    evaluate(vfq.object);
    os_ << "[";
    if (!vfq.propertyPath.empty()) {
      auto it = std::begin(vfq.propertyPath);
      _eval(it->get());
      while (++it != std::end(vfq.propertyPath)) {
        os_ << '/';
        _eval(it->get());
      }
    } else {
      os_ << bookMarkError(strJoin("Empty property path"));
    }
    os_ << "]";
  }

  void eval(const Variable& rv) override {
    os_ << prefix::variable;
    if (rv.name.empty()) {
      os_ << bookMarkError(JASSTR("a `Property Name` must not be empty"));
    } else {
      os_ << rv.name;
    }
  }

  void eval(const ContextArgument& arg) override {
    os_ << prefix::variable << arg.index;
  }

  void eval(const ContextArgumentsInfo& arginf) override {
    switch (arginf.type) {
      case ContextArgumentsInfo::Type::ArgCount:
        os_ << prefix::variable << prefix::argc;
        break;
      case ContextArgumentsInfo::Type::Args:
        os_ << prefix::variable << prefix::args;
        break;
    }
  }

  void _eval(const Evaluable* evb) {
    if (evb) {
      if (evb->useStack()) {
        auto shouldClose = false;
        if (_evalStackSymbols(static_cast<const UseStackEvaluable&>(*evb))) {
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

String SyntaxValidator::syntaxOf(const Evaluable& e) {
  return SyntaxValidator{}.generateSyntax(e);
}

String SyntaxValidator::syntaxOf(const EvaluablePtr& e) {
  return SyntaxValidator{}.generateSyntax(e);
}
}  // namespace jas
