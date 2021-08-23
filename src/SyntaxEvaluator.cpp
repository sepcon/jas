#include "jas/SyntaxEvaluator.h"

#include "jas/SyntaxEvaluatorImpl.h"

namespace jas {

SyntaxEvaluator::SyntaxEvaluator() : impl_{new SyntaxEvaluatorImpl} {}

SyntaxEvaluator::~SyntaxEvaluator() { delete impl_; }

EvaluatedValue SyntaxEvaluator::evaluate(const Evaluable& e,
                                         EvalContextPtr rootContext) {
  return impl_->evaluate(e, move(rootContext));
}

EvaluatedValue SyntaxEvaluator::evaluate(const EvaluablePtr& e,
                                         EvalContextPtr rootContext) {
  return impl_->evaluate(e, move(rootContext));
}
void SyntaxEvaluator::setDebugInfoCallback(
    DebugOutputCallback debugOutputCallback) {
  impl_->setDebugInfoCallback(move(debugOutputCallback));
}

}  // namespace jas
