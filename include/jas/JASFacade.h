#pragma once

#include "EvalContextIF.h"
#include "Evaluable.h"
#include "FunctionModule.h"

namespace jas {
class Translator;
class SyntaxEvaluator;
class ModuleManager;

class JASFacade {
 public:
  JASFacade();
  JASFacade(const Json& jasExpr, EvalContextPtr context);
  ~JASFacade();
  Var evaluate(const Json& jasExpr, EvalContextPtr context);
  Var evaluate();

  // For evaluating other context/expression
  void setContext(EvalContextPtr context) noexcept;
  void setExpression(const Json& jasExpr);

  // Extend ability of evaluator
  bool addModule(FunctionModulePtr module) noexcept;
  bool removeModule(const FunctionModulePtr& module) noexcept;
  bool removeModule(const String& moduleName) noexcept;

  // For showing help and debugging
  String getTransformedSyntax() noexcept;
  FunctionNameList functionList(const EvalContextPtr& context = {}) noexcept;
  void setDebugCallback(std::function<void(const String&)> callback) noexcept;

  // For further custom work
  SyntaxEvaluator* getEvaluator() noexcept;
  Translator* getParser() noexcept;
  ModuleManager* getModuleMgr() noexcept;

 private:
  struct _JASFacade* d_ = nullptr;
};
}  // namespace jas
