#pragma once

#include <functional>

#include "EvalContextIF.h"
#include "Evaluable.h"

namespace jas {

using DebugOutputCallback = std::function<void(const String&)>;
using EvaluatedValue = std::shared_ptr<JsonAdapter>;
struct Evaluable;

class SyntaxEvaluator {
 public:
  SyntaxEvaluator();
  ~SyntaxEvaluator();
  EvaluatedValue evaluate(const Evaluable& e,
                          EvalContextPtr rootContext = nullptr);
  EvaluatedValue evaluate(const EvaluablePtr& e,
                          EvalContextPtr rootContext = nullptr);

  void setDebugInfoCallback(DebugOutputCallback);

 private:
  class SyntaxEvaluatorImpl* impl_ = nullptr;
};
}  // namespace jas
