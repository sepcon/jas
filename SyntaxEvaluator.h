#pragma once

#include "EvalContextIF.h"
#include "Evaluable.h"

namespace jas {

using EvaluatedValue = JsonAdapter;
struct Evaluable;

class SyntaxEvaluator {
 public:
  SyntaxEvaluator();
  ~SyntaxEvaluator();
  EvaluatedValue evaluatedResult() const;
  EvaluatedValue evaluate(const Evaluable& e,
                          EvalContextPtr rootContext = nullptr);
  EvaluatedValue evaluate(const EvaluablePtr& e,
                          EvalContextPtr rootContext = nullptr);

 private:
  class SyntaxEvaluatorImpl* impl_ = nullptr;
};
}  // namespace jas
