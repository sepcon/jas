#pragma once

#include "Json.h"

namespace jas {
class SyntaxEvaluatorImpl;
class FunctionInvocation;
class FunctionModuleIF;
using FunctionModulePtr = std::shared_ptr<FunctionModuleIF>;
using JsonAdapterPtr = std::shared_ptr<JsonAdapter>;

class FunctionModuleIF {
 public:
  virtual ~FunctionModuleIF() = default;
  virtual JsonAdapterPtr eval(const FunctionInvocation& fi,
                              SyntaxEvaluatorImpl*) = 0;
};

bool registerModule(const String& moduleName, FunctionModulePtr mdl);
bool unregisterModule(const String& moduleName);
JsonAdapterPtr eval(const FunctionInvocation& fi, SyntaxEvaluatorImpl*);

}  // namespace jas
