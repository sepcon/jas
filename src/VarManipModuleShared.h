#pragma once

#include "jas/FunctionModuleBaseT.h"

namespace jas {
namespace mdl {

using ModuleFunction = Var (*)(Var params);

inline Var &variable_detach(Var &var) {
  var.detach();
  return var;
}

#define __module_func(module, func_name, params)                    \
  static const auto module##_func_##func_name = JASSTR(#func_name); \
  Var module##_##func_name(Var params)

#define __module_class_begin(module)                               \
  struct __FunctionModule##module                                  \
      : public FunctionModuleBaseT<ModuleFunction> {               \
    Var invoke(const ModuleFunction &func, EvaluablePtr param,     \
               SyntaxEvaluatorImpl *evaluator) override {          \
      return func(evaluator->evalAndReturn(param.get()));               \
    }                                                              \
    String moduleName() const override { return JASSTR(#module); } \
    const FunctionsMap &_funcMap() const override {                \
      const static FunctionsMap _ =

#define __module_class_end(module)                                     \
  ;                                                                    \
  return _;                                                            \
  }                                                                    \
  }                                                                    \
  ;                                                                    \
  FunctionModulePtr getModule() {                                      \
    static auto module = std::make_shared<__FunctionModule##module>(); \
    return module;                                                     \
  }

#define __module_register_func(module, func_name) \
  { module##_func_##func_name, module##_##func_name }

}  // namespace mdl
}  // namespace jas
