#include "EvalContextBase.h"

#include "CIF.h"
#include "Exception.h"
#include "Keywords.h"

namespace jas {

using std::make_shared;
using std::move;

static bool _isGloblProperty(const String &name);

EvalContextBase::EvalContextBase(EvalContextBase *parent, String id)
    : parent_(parent), id_(move(id)) {}

bool EvalContextBase::functionSupported(const String &functionName) const {
  if (parent_) {
    return parent_->functionSupported(functionName);
  }
  return false;
}

JsonAdapter EvalContextBase::invoke(const String &funcName,
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

JAdapterPtr EvalContextBase::property(const String &name) const {
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
    } else {
      throw_<EvaluationError>("Property not found: ", name);
    }
  } while (false);
  return prop;
}

void EvalContextBase::setProperty(const String &name, JAdapterPtr val) {
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

String EvalContextBase::debugInfo() const {
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

const EvalContextBase *EvalContextBase::rootContext() const {
  auto root = this;
  while (root->parent_) {
    root = root->parent_;
  }
  return root;
}

EvalContextBase *EvalContextBase::rootContext() {
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
