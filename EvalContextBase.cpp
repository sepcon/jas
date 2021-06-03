#include "EvalContextBase.h"

#include "CIF.h"
#include "Exception.h"
#include "Keywords.h"

namespace jas {

using std::move;

static bool _isGloblProperty(const String &name);

EvalContextBase::EvalContextBase(EvalContextBase *parent, String id)
    : parent_(parent), id_(move(id)) {}

std::vector<String> EvalContextBase::supportedFunctions() const {
  return cif::supportedFunctions();
}

bool EvalContextBase::functionSupported(const String &functionName) const {
  if (parent_) {
    return parent_->functionSupported(functionName);
  } else {
    return cif::supported(functionName);
  }
}

JsonAdapter EvalContextBase::invoke(const String &funcName,
                                    const JsonAdapter &param) {
  if (parent_) {
    try {
      return parent_->invoke(funcName, param);
    } catch (const FunctionNotFoundError &) {
    }
  }
  return cif::invoke(funcName, param);
}

JsonAdapter EvalContextBase::property(const String &name) const {
  JsonAdapter prop;
  do {
    if (_isGloblProperty(name)) {
      auto root = rootContext();
      if (root == this) {
        if (auto it = properties_.find(name.c_str() + 1);
            it != properties_.end()) {
          prop = it->second;
        }
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

void EvalContextBase::setProperty(const String &name, JsonAdapter val) {
  if (_isGloblProperty(name)) {
    // This case for global property
    if (auto root = rootContext(); root == this) {
      properties_[name.substr(1)] = move(val.value);
    } else {
      root->setProperty(name.substr(1), move(val));
    }
  } else {
    properties_[name] = move(val.value);
  }
}

String EvalContextBase::debugInfo() const { return id_; }

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
