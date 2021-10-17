#pragma once

#include "EvaluationFrame.h"

namespace jas {

struct UseStackEvaluable;
class EvaluationStack {
 public:
  EvaluationStack();
  ~EvaluationStack();

  void push(String ctxtID, const Evaluable* evb,
            ContextArguments contextData = {});
  void init(EvalContextPtr rootContext, const Evaluable* e);
  void pop();
  const EvaluationFramePtr& top() const;
  EvaluationFramePtr top();
  void top(EvaluationFramePtr frame);
  void return_(Var val);
  void return_(Var val, const UseStackEvaluable& ev);
  Var& returnedVal();
  Var takeReturnedVal();

  String dump() const;
  void clear();

 private:
  int size() const;
  EvaluationFramePtr topFrame;
};

}  // namespace jas
