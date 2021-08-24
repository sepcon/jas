#include "jas/BasicEvalContext.h"

#include "jas/Exception.h"
#include "jas/FunctionModule.h"
#include "jas/Keywords.h"

namespace jas {

using std::make_shared;
using std::move;

static bool _isGloblProperty(const String &name);

BasicEvalContext::BasicEvalContext(BasicEvalContext *parent, String id)
    : parent_(parent), id_(move(id)) {}

std::vector<String> BasicEvalContext::supportedFunctions() const {
  return {};
}

bool BasicEvalContext::functionSupported(
    const StringView &functionName) const {
  if (parent_) {
    return parent_->functionSupported(functionName);
  }
  return false;
}

Var BasicEvalContext::invoke(const String &funcName, const Var &param) {
  if (parent_) {
    try {
      return parent_->invoke(funcName, param);
    } catch (const FunctionNotFoundError &) {
    }
  }
  throw_<FunctionNotFoundError>();
  return {};
}

Var BasicEvalContext::variable(const String &name) {
  if (_isGloblProperty(name)) {
    auto root = rootContext();
    if (root == this) {
      auto it = variables_.find(name.c_str() + 1);
      if (it != variables_.end()) {
        return it->second;
      }
    }
    return root->variable(name.substr(1));
  }
  auto it = variables_.find(name);
  if (it != variables_.end()) {
    return it->second;
  } else if (parent_) {
    return parent_->variable(name);
  }
  throw_<EvaluationError>("Property not found: ", name);
  return {};
}

Var BasicEvalContext::setVariable(const String &name, Var val) {
  if (_isGloblProperty(name)) {
    // This case for global property
    if (auto root = rootContext(); root == this) {
      return variables_[name.substr(1)] = move(val);
    } else {
      return root->setVariable(name.substr(1), move(val));
    }
  } else {
    return variables_[name] = move(val);
  }
}

EvalContextPtr BasicEvalContext::subContext(const String &ctxtID,
                                               const Var &) {
  return make_shared<BasicEvalContext>(this, ctxtID);
}

Var BasicEvalContext::findProperty(BasicEvalContext *ctxt,
                                      const String &name) {
  if (auto it = ctxt->variables_.find(name); it != std::end(ctxt->variables_)) {
    return it->second;
  } else {
    return {};
  }
}

String BasicEvalContext::debugInfo() const {
  auto inf = id_;
  OStringStream oss;
  oss << id_ << "({";
  for (auto &[prop, val] : variables_) {
    oss << "{" << prop << ", ";
    oss << val.dump();
    oss << " }, ";
  }
  oss << "})";
  return oss.str();
}

const BasicEvalContext *BasicEvalContext::rootContext() const {
  auto root = this;
  while (root->parent_) {
    root = root->parent_;
  }
  return root;
}

BasicEvalContext *BasicEvalContext::rootContext() {
  auto root = this;
  while (root->parent_) {
    root = root->parent_;
  }
  return root;
}

static bool _isGloblProperty(const String &name) {
  return name[0] == prefix::variable;
}

}  // namespace jas
