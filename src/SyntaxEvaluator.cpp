#include "jas/SyntaxEvaluator.h"

#include "jas/SyntaxEvaluatorImpl.h"

namespace jas {

SyntaxEvaluator::SyntaxEvaluator(ModuleManager* moduleMgr)
    : impl_{new SyntaxEvaluatorImpl(moduleMgr)} {}

SyntaxEvaluator::~SyntaxEvaluator() { delete impl_; }

Var SyntaxEvaluator::evaluate(const Evaluable& e,
                                 EvalContextPtr rootContext) {
  return impl_->evaluate(e, move(rootContext));
}

Var SyntaxEvaluator::evaluate(const EvaluablePtr& e,
                                 EvalContextPtr rootContext) {
  return impl_->evaluate(e, move(rootContext));
}
void SyntaxEvaluator::setDebugInfoCallback(
    DebugOutputCallback debugOutputCallback) {
  impl_->setDebugInfoCallback(move(debugOutputCallback));
}

}  // namespace jas
