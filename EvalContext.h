#pragma once

#include <vector>

#include "EvaluableClasses.h"

namespace jas {
class EvalContextBase;
using EvalContextPtr = std::shared_ptr<EvalContextBase>;
using EvalContexts = std::vector<EvalContextPtr>;

class EvalContextBase {
 public:
  using Snapshot = ContextVal::Snapshot;
  virtual ~EvalContextBase() = default;
  virtual DirectVal invokeFunction(const String& name,
                                   const DirectVal& param) = 0;
  virtual DirectVal snapshotValue(const String& path,
                                  Snapshot snapshot) const = 0;
  virtual DirectVal getProperty(const String& refid) const = 0;
  virtual void setProperty(const String& path, DirectVal val) = 0;
  virtual EvalContexts subContexts(const DirectVal& listSub,
                                   const String& itemID = {}) = 0;
  virtual String debugInfo() const { return ""; }
};

}  // namespace jas
