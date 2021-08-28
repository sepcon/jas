#pragma once

#include "FunctionModule.h"

namespace jas {

class ModuleManager {
 public:
  using ModuleMap = std::map<String, FunctionModulePtr, std::less<>>;
  bool addModule(const String& moduleName, FunctionModulePtr mdl) noexcept;
  bool removeModule(const String& moduleName) noexcept;
  bool hasModule(const StringView& moduleName) const noexcept;
  FunctionModulePtr getModule(const StringView& moduleName);
  FunctionModulePtr findModuleByFuncName(const StringView& funcName);
  bool hasFunction(const StringView& moduleName,
                   const StringView& funcName) noexcept;
  FunctionNameList enumerateFuncions();
  Var invoke(const String& module, const String& funcName, Var& evaluatedParam,
             SyntaxEvaluatorImpl*);
  const ModuleMap& modules() const { return modules_; }

 private:
  ModuleMap modules_;
};

}  // namespace jas
