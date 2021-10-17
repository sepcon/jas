#pragma once

#include "Evaluable.h"
#include "jas/Var.h"

namespace jas {

class SyntaxEvaluatorImpl;
class FunctionModuleIF;
using FunctionNameList = std::vector<String>;
using FunctionModulePtr = std::shared_ptr<FunctionModuleIF>;

class FunctionModuleIF {
 public:
  virtual ~FunctionModuleIF() = default;
  virtual String moduleName() const = 0;
  virtual Var eval(const String& funcName, EvaluablePtr param,
                   SyntaxEvaluatorImpl*) = 0;
  virtual bool has(const StringView& funcName) const = 0;
  virtual void enumerateFuncs(FunctionNameList& funcName) const = 0;
};

__mc_jas_exception(FunctionNotFoundError);
}  // namespace jas

#define __module_creating_prototype(module_name) \
  namespace jas {                                \
  namespace mdl {                                \
  namespace module_name {                        \
  FunctionModulePtr getModule();                 \
  }                                              \
  }                                              \
  }

/// No input function throw
#define __jas_ni_func_throw_invalidargs(errmsg) \
  throw_<InvalidArgument>("function `", __func__, "`: ", errmsg)

/// Having input function throw
#define __jas_func_throw_invalidargs(errmsg, jparam)             \
  throw_<InvalidArgument>("function `", __func__, "`: ", errmsg, \
                          ", given: ", jparam)

#define __jas_ni_func_throw_invalidargs_if(cond, errmsg) \
  __jas_throw_if(InvalidArgument, cond, "function `", __func__, "`: ", errmsg)

#define __jas_func_throw_invalidargs_if(cond, errmsg, jparam)                  \
  __jas_throw_if(InvalidArgument, cond, "function `", __func__, "`: ", errmsg, \
                 ", given: ", jparam)
