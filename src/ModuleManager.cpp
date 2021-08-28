#include "jas/ModuleManager.h"

namespace jas {

using std::move;

bool ModuleManager::addModule(const String &moduleName,
                              FunctionModulePtr mdl) noexcept {
  auto &tobePut = modules_[moduleName];
  if (!tobePut) {
    tobePut = move(mdl);
    return true;
  }
  return false;
}

bool ModuleManager::removeModule(const String &moduleName) noexcept {
  return modules_.erase(moduleName) != 0;
}

bool ModuleManager::hasModule(const StringView &moduleName) const noexcept {
  return modules_.find(moduleName) != std::end(modules_);
}

bool ModuleManager::hasFunction(const StringView &moduleName,
                                const StringView &funcName) noexcept {
  auto it = modules_.find(moduleName);
  return it != std::end(modules_) && it->second->has(funcName);
}

FunctionModulePtr ModuleManager::getModule(const StringView &moduleName) {
  auto it = modules_.find(moduleName);
  return it != std::end(modules_) ? it->second : FunctionModulePtr{};
}

FunctionModulePtr ModuleManager::findModuleByFuncName(
    const StringView &funcName) {
  FunctionModulePtr ret;
  for (auto &[mname, module] : modules_) {
    if (module->has(funcName)) {
      if (ret) {
        throw_<Exception>("Ambiguous call to function `", funcName,
                          "` that can be found in module `", ret->moduleName(),
                          "` and `", module->moduleName(), "`");
      }
      ret = module;
    }
  }
  return ret;
}

FunctionNameList ModuleManager::enumerateFuncions() {
  FunctionNameList list;
  for (auto &[moduleName, module] : modules_) {
    module->enumerateFuncs(list);
  }
  return list;
}

Var ModuleManager::invoke(const String &module, const String &funcName,
                          Var &evaluatedParam, SyntaxEvaluatorImpl *evaluator) {
  auto itMdl = modules_.find(module);
  __jas_throw_if(FunctionNotFoundError, itMdl == std::end(modules_),
                 "Not found module ", module);
  assert(itMdl->second);
  return itMdl->second->eval(funcName, evaluatedParam, evaluator);
}

}  // namespace jas
