#pragma once

#include <map>

#include "EvalContextStandard.h"
#include "String.h"

namespace jas {

class HistoricalEvalContext : public EvalContextStandard {
  using _Base = EvalContextStandard;

 public:
  using EvaluationResult = JsonObject;
  using EvaluationResultPtr = std::shared_ptr<EvaluationResult>;
  enum SnapshotIdx {
    SnapshotIdxOld = 0,
    SnapshotIdxNew = 1,
    SnapshotIdxMax,
  };

  HistoricalEvalContext(HistoricalEvalContext* p = nullptr,
                        Json currentSnapshot = {}, Json lastSnapshot = {},
                        String id = {});
  ~HistoricalEvalContext();

  const EvaluationResultPtr& lastEvalResult();
  void setLastEvalResult(EvaluationResultPtr res);
  void syncEvalResult();
  bool saveEvaluationResult(OStream& ostrm);
  bool loadEvaluationResult(IStream& istrm);
  std::vector<String> supportedFunctions() const override;
  bool functionSupported(const StringView& functionName) const override;
  JsonAdapter invoke(const String& funcName, const JsonAdapter& param) override;

 private:
  Json snapshots_[SnapshotIdxMax];
  EvaluationResultPtr lastEvalResult_;

  Json snapshotValue(const String& path, const String& snapshot) const;
  Json snapshotValue(const String& path,
                     const SnapshotIdx snidx = SnapshotIdxNew) const;
  EvalContextPtr subContext(const String& ctxtID,
                            const JsonAdapter& input) override;
  bool hasData() const;
  HistoricalEvalContext* parent() const;
  String contextPath() const;
  String contextPath(const String& variableName) const;

  // context invocable methods
  /// check snapshot changed
  JsonAdapter snchg(const Json& jpath);
  /// check whether evaluated value differs with its last evaluation or not
  JsonAdapter evchg(const Json& json);
  JsonAdapter field(const Json& params);
  JsonAdapter field_lv(const Json& path);
  JsonAdapter field_cv(const Json& path);
  JsonAdapter hfield2arr(const Json& params);
  JsonAdapter hfield(const Json& params);
  JsonAdapter last_eval(const Json& jVarName);
  using CtxtFuncPtr = JsonAdapter (HistoricalEvalContext::*)(const Json&);
  using CtxtFunctionsMap = std::map<String, CtxtFuncPtr, std::less<>>;

  static CtxtFunctionsMap funcsmap_;
};

}  // namespace jas
