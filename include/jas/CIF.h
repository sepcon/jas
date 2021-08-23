/// Context Independent Functions
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "Json.h"

namespace jas {

__mc_jas_exception(FunctionNotFoundError);
__mc_jas_exception(FunctionInvalidArgTypeError);
__mc_jas_exception(FunctionNotImplementedError);

/// context independent function
namespace cif {
using FunctionSupportedCheck = std::function<bool(const StringView&)>;
using ModuleFunctionInvoke =
    std::function<JsonAdapter(const String&, const JsonAdapter&)>;
using FunctionListCallback = std::function<void(std::vector<String>&)>;

struct Module {
  FunctionSupportedCheck supported;
  ModuleFunctionInvoke invoke;
  FunctionListCallback listFuncs;
};

std::vector<String> supportedFunctions();
bool supported(const StringView& moduleName, const StringView& funcName);
JsonAdapter invoke(const String& module, const String& funcName,
                   const JsonAdapter& e = {});
void registerModule(const String& moduleName, Module mdl);

}  // namespace cif
}  // namespace jas

#define __jas_ni_func_throw_invalidargs(errmsg) \
  throw_<InvalidArgument>("function `", __func__, "`: ", errmsg)

#define __jas_func_throw_invalidargs(errmsg, jparam)             \
  throw_<InvalidArgument>("function `", __func__, "`: ", errmsg, \
                          ", given: ", JsonTrait::dump(jparam))

#define __jas_ni_func_throw_invalidargs_if(cond, errmsg) \
  throwIf<InvalidArgument>(cond, "function `", __func__, "`: ", errmsg)

#define __jas_func_throw_invalidargs_if(cond, errmsg, jparam)           \
  throwIf<InvalidArgument>(cond, "function `", __func__, "`: ", errmsg, \
                           ", given: ", JsonTrait::dump(jparam))
