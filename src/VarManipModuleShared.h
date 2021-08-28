#pragma once

#include "jas/FunctionModuleBaseT.h"

namespace jas {
namespace mdl {

using std::make_shared;
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
    String moduleName() const override { return JASSTR(#module); } \
    const FunctionsMap &_funcMap() const override {                \
      const static FunctionsMap _ =

#define __module_class_end(module)                                \
  ;                                                               \
  return _;                                                       \
  }                                                               \
  }                                                               \
  ;                                                               \
  FunctionModulePtr getModule() {                                 \
    static auto module = make_shared<__FunctionModule##module>(); \
    return module;                                                \
  }

#define __module_register_func(module, func_name) \
  { module##_func_##func_name, module##_##func_name }

}  // namespace mdl
}  // namespace jas
