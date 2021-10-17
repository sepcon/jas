#include "EvaluationStack.h"

#include "jas/EvaluableClasses.h"
#include "jas/SyntaxValidator.h"

namespace jas {
using std::make_shared;
using std::move;

EvaluationStack::EvaluationStack() {}

EvaluationStack::~EvaluationStack() {}

void EvaluationStack::push(String ctxtID, const Evaluable *evb,
                           ContextArguments contextData) {
  auto subCtxt = topFrame->context->subContext(ctxtID, move(contextData));
  topFrame =
      make_shared<EvaluationFrame>(move(topFrame), move(subCtxt), Var{}, evb);
}

void EvaluationStack::init(EvalContextPtr rootContext, const Evaluable *e) {
  topFrame.reset();
  topFrame = make_shared<EvaluationFrame>(EvaluationFramePtr{},
                                          move(rootContext), Var{}, e);
}

void EvaluationStack::pop() { topFrame = topFrame->parent; }

const EvaluationFramePtr &EvaluationStack::top() const { return topFrame; }

EvaluationFramePtr EvaluationStack::top() { return topFrame; }

void EvaluationStack::top(EvaluationFramePtr frame) { topFrame = move(frame); }

void EvaluationStack::return_(Var val) { topFrame->returnedValue = move(val); }

void EvaluationStack::return_(Var val, const UseStackEvaluable &ev) {
  if (!ev.id.empty()) {
    topFrame->context->putVariable(ev.id, val);
  }
  return_(move(val));
}

Var &EvaluationStack::returnedVal() { return topFrame->returnedValue; }

Var EvaluationStack::takeReturnedVal() { return move(topFrame->returnedValue); }

String EvaluationStack::dump() const {
  OStringStream oss;
  oss << std::boolalpha;
  int i = size();
  auto currentFrame = topFrame;
  while (currentFrame) {
    oss << "#" << std::left << std::setw(3) << i-- << ": " << std::setw(60)
        << (currentFrame->evb ? SyntaxValidator::syntaxOf(*currentFrame->evb)
                              : JASSTR("----------##--------"));
    if (auto ctxinfo = currentFrame->context->debugInfo(); !ctxinfo.empty()) {
      oss << " --> [ctxt]: " << currentFrame->context->debugInfo();
    }
    oss << "\n";
    currentFrame = currentFrame->parent;
  }
  return oss.str();
}

void EvaluationStack::clear() { topFrame.reset(); }

int EvaluationStack::size() const {
  auto currentFrame = topFrame;
  auto stackSize = 0;
  while (currentFrame) {
    ++stackSize;
    currentFrame = currentFrame->parent;
  }
  return stackSize;
}

}  // namespace jas
