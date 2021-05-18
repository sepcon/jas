#pragma once

#include "EvalContext.h"
#include "EvaluableClasses.h"
#include "Invocable.h"
#include "String.h"

namespace jas {

class JEvalContext : public EvalContextBase {
 public:
  using PropertyMap = std::map<String, Json>;
  using EvalHistory = std::map<String, Json>;
  using EvalHistoryPtr = std::shared_ptr<EvalHistory>;
  JEvalContext(JEvalContext* parent = nullptr, Json new_snapshot = {},
               Json old_snapshot = {});
  DirectVal invokeFunction(const String& funcName,
                           const DirectVal& param = {}) override;
  DirectVal snapshotValue(const String& path, Snapshot snapshot) const override;
  DirectVal getProperty(const String& name) const override;
  void setProperty(const String& name, DirectVal val) override;
  EvalContexts subContexts(const DirectVal& listSub,
                           const String& itemId) override;

  JEvalContext* rootContext();
  String contextPath() const;
  const EvalHistoryPtr& evalHistory();
  void setEvalHistory(EvalHistoryPtr history);

  // context invocable methods
  /// check snapshot changed
  bool snchg() const;
  /// check whether evaluated value differs with its last evaluation or not
  bool evchg(const DirectVal& evaluated);

  String id_;
  JEvalContext* parent_ = nullptr;
  Json snapshots_[2];
  PropertyMap properties_;
  FunctionsMap funcsmap_;
  EvalHistoryPtr evalHistory_;
};

}  // namespace jas
