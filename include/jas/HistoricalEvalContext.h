#pragma once

#include <map>

#include "BasicEvalContext.h"
#include "String.h"

namespace jas {

class HistoricalEvalContext : public BasicEvalContext {
  using _Base = BasicEvalContext;

 public:
  using EvaluatedVariables = Var;
  using EvaluatedVariablesPtr = std::shared_ptr<EvaluatedVariables>;
  enum SnapshotIdx {
    SnapshotIdxOld = 0,
    SnapshotIdxNew = 1,
    SnapshotIdxMax,
  };

  static const constexpr auto TemporaryVariablePrefix = JASSTR('.');

  HistoricalEvalContext(HistoricalEvalContext* p = nullptr,
                        Var currentSnapshot = {}, Var lastSnapshot = {},
                        String id = {});
  ~HistoricalEvalContext();

  static std::shared_ptr<HistoricalEvalContext> make(
      HistoricalEvalContext* p = nullptr, Var currentSnapshot = {},
      Var lastSnapshot = {}, String id = {});

  const EvaluatedVariablesPtr& lastEvalResult();
  void setLastEvalResult(EvaluatedVariablesPtr res);
  void syncEvalResult();
  bool saveEvaluationResult(OStream& ostrm);
  bool loadEvaluationResult(IStream& istrm);
  std::vector<String> supportedFunctions() const override;
  bool functionSupported(const StringView& functionName) const override;
  Var invoke(const String& funcName, const Var& param) override;

 private:
  Var snapshots_[SnapshotIdxMax];
  EvaluatedVariablesPtr lastEvalResult_;

  Var snapshotValue(const String& path, const String& snapshot) const;
  Var snapshotValue(const String& path,
                    const SnapshotIdx snidx = SnapshotIdxNew) const;
  EvalContextPtr subContext(const String& ctxtID, const Var& input) override;
  bool hasData() const;
  HistoricalEvalContext* parent() const;
  String contextPath() const;
  String contextPath(const String& variableName) const;

  // context invocable methods
  /// check snapshot changed
  Var snchg(const Var& jpath);
  /// check whether evaluated value differs with its last evaluation or not
  Var evchg(const Var& json);
  Var field(const Var& params);
  Var field_lv(const Var& path);
  Var field_cv(const Var& path);
  Var hfield2arr(const Var& params);
  Var hfield(const Var& params);
  Var last_eval(const Var& jVarName);
  Var _hfield(const String& path, const String& iid);

  using CtxtFuncPtr = Var (HistoricalEvalContext::*)(const Var&);
  using CtxtFunctionsMap = std::map<String, CtxtFuncPtr, std::less<>>;

  static CtxtFunctionsMap funcsmap_;
};

}  // namespace jas
