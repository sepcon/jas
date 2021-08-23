#pragma once

#include "jas/Json.h"

namespace jas {
class SyntaxEvaluatorImpl;
class FunctionInvocation;
namespace builtin {

class FIEDelegator {
 public:
  virtual ~FIEDelegator() = default;
  virtual std::shared_ptr<JsonAdapter> eval(const FunctionInvocation& fi,
                                            SyntaxEvaluatorImpl*) = 0;
};

}  // namespace builtin
}  // namespace jas
