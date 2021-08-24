#pragma once

#include <functional>

#include "EvalContextIF.h"
#include "Evaluable.h"
#include "Var.h"

namespace jas {

using DebugOutputCallback = std::function<void(const String&)>;
class ModuleManager;
class Evaluable;

class SyntaxEvaluator {
 public:
  SyntaxEvaluator(ModuleManager* moduleMgr);
  ~SyntaxEvaluator();
  Var evaluate(const Evaluable& e, EvalContextPtr rootContext = nullptr);
  Var evaluate(const EvaluablePtr& e, EvalContextPtr rootContext = nullptr);

  void setDebugInfoCallback(DebugOutputCallback);

 private:
  class SyntaxEvaluatorImpl* impl_ = nullptr;
};
}  // namespace jas
