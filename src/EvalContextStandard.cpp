#include "jas/EvalContextStandard.h"

#include "jas/CIF.h"
#include "jas/Exception.h"
#include "jas/Keywords.h"

namespace jas {

using std::make_shared;
using std::move;

static bool _isGloblProperty(const String &name);

EvalContextStandard::EvalContextStandard(EvalContextStandard *parent, String id)
    : parent_(parent), id_(move(id)) {}

bool EvalContextStandard::functionSupported(
    const StringView &functionName) const {
  if (parent_) {
    return parent_->functionSupported(functionName);
  }
  return false;
}

JsonAdapter EvalContextStandard::invoke(const String &funcName,
                                        const JsonAdapter &param) {
  if (parent_) {
    try {
      return parent_->invoke(funcName, param);
    } catch (const FunctionNotFoundError &) {
    }
  }
  throw_<FunctionNotFoundError>();
  return {};
}

JAdapterPtr EvalContextStandard::property(const String &name) const {
  JAdapterPtr prop;
  do {
    if (_isGloblProperty(name)) {
      auto root = rootContext();
      if (root == this) {
        auto it = properties_.find(name.c_str() + 1);
        throwIf<EvaluationError>(!(it != properties_.end()),
                                 "Property not found: ", name);
        prop = it->second;
        break;
      }
      prop = root->property(name.substr(1));
      break;
    }
    auto it = properties_.find(name);
    if (it != properties_.end()) {
      prop = it->second;
    } else if (parent_) {
      prop = parent_->property(name);
    }
  } while (false);
  return prop;
}

void EvalContextStandard::setProperty(const String &name, JAdapterPtr val) {
  if (_isGloblProperty(name)) {
    // This case for global property
    if (auto root = rootContext(); root == this) {
      properties_[name.substr(1)] = move(val);
    } else {
      root->setProperty(name.substr(1), move(val));
    }
  } else {
    properties_[name] = move(val);
  }
}

JAdapterPtr *EvalContextStandard::findProperty(EvalContextStandard *ctxt,
                                               const String &name) {
  if (auto it = ctxt->properties_.find(name);
      it != std::end(ctxt->properties_)) {
    return &(it->second);
  } else {
    return nullptr;
  }
}

String EvalContextStandard::debugInfo() const {
  auto inf = id_;
  OStringStream oss;
  oss << id_ << "({";
  for (auto &[prop, val] : properties_) {
    oss << "{" << prop << ", ";
    if (val) {
      oss << val->dump();
    } else {
      oss << "null";
    }
    oss << " }, ";
  }
  oss << "})";
  return oss.str();
}

const EvalContextStandard *EvalContextStandard::rootContext() const {
  auto root = this;
  while (root->parent_) {
    root = root->parent_;
  }
  return root;
}

EvalContextStandard *EvalContextStandard::rootContext() {
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
