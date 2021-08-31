#pragma once

#include <jas/Var.h>

#include <filesystem>
#include <functional>

namespace owc {

enum class SOHEvaluationStatus {
  Success,

};

class SOHEvaluator {
 public:
  using Json = jas::Json;
  using LastEvaluatedVariables = jas::Var;
  using LogCallback = std::function<void(const jas::String&)>;
  using FSPath = std::filesystem::path;

  SOHEvaluator(FSPath workingDir, Json evaluationGuide);
  bool evaluate(Json report);
  void setLogCallback(LogCallback logcb);
  void setDebugCallback(LogCallback logcb);
  void setWorkingDir(FSPath workingDir);
  bool shouldReport() const;
  const LastEvaluatedVariables& lastEvalDetails() const;
  bool saveEvalHistory() const;

 private:
  bool _evaluate(Json report);
  void _cleanUpEvalHistory();
  bool _loadEvalHistory();
  FSPath workingDir_;
  Json evaluationGuide_;
  Json lastReport_;
  LastEvaluatedVariables lastEvals_;
  LogCallback logCallback_;
  LogCallback debugCallback_;
  bool shouldReport_ = true;
};

}  // namespace owc
