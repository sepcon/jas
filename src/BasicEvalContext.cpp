#include "jas/BasicEvalContext.h"

#include "jas/Exception.h"
#include "jas/FunctionModule.h"
#include "jas/Keywords.h"

namespace jas {

using std::make_shared;
using std::move;

static bool _isGloblProperty(const String &name);

BasicEvalContext::BasicEvalContext(BasicEvalContext *parent, String id,
                                   ContextArguments input)
    : parent_(parent), id_(move(id)), args_(move(input)) {}

std::vector<String> BasicEvalContext::supportedFunctions() const { return {}; }

bool BasicEvalContext::functionSupported(const StringView &functionName) const {
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

Var *BasicEvalContext::lookupVariable(const String &name) {
  if (_isGloblProperty(name)) {
    auto root = rootContext();
    if (root == this) {
      auto it = variables_.find(name.c_str() + 1);
      if (it != variables_.end()) {
        return &it->second;
      }
    }
    return root->lookupVariable(name.substr(1));
  }
  auto it = variables_.find(name);
  if (it != variables_.end()) {
    return &it->second;
  } else if (parent_) {
    return parent_->lookupVariable(name);
  }
  return nullptr;
}

Var *BasicEvalContext::putVariable(const String &name, Var val) {
  auto makeVar = [&val] {
    if (val.isRef()) {
      return Var::ref(*val.asRef());
    } else {
      return Var::ref(move(val));
    }
  };
  if (_isGloblProperty(name)) {
    // This case for global property
    if (auto root = rootContext(); root == this) {
      return &(variables_[name.substr(1)] = makeVar());
    } else {
      return root->putVariable(name.substr(1), makeVar());
    }
  } else {
    return &(variables_[name] = makeVar());
  }
}

EvalContextPtr BasicEvalContext::subContext(const String &ctxtID,
                                            ContextArguments args) {
  return make_shared<BasicEvalContext>(this, ctxtID, move(args));
}

String BasicEvalContext::debugInfo() const { return id_; }

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

Var BasicEvalContext::arg(uint8_t pos) const noexcept {
  if (!args_.empty()) {
    if (pos > 0 && pos <= args_.size()) {
      return args_[pos - 1];
    } else {
      return {};
    }
  } else if (parent_) {
    return parent_->arg(pos);
  } else {
    return {};
  }
}

void BasicEvalContext::args(ContextArguments args) { args_ = move(args); }

const ContextArguments &BasicEvalContext::args() const noexcept {
  if (args_.empty() && parent_) {
    return parent_->args();
  } else {
    return args_;
  }
}

}  // namespace jas
