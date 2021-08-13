/// Context Independent Functions
#pragma once

#include <functional>
#include <map>
#include <vector>

#include "Json.h"

namespace jas {

using JasInvokable = std::function<JsonAdapter(const JsonAdapter&)>;
using FunctionsMap = std::map<String, JasInvokable>;
__mc_jas_exception(FunctionNotFoundError);
__mc_jas_exception(FunctionInvalidArgTypeError);
__mc_jas_exception(FunctionNotImplementedError);

JsonAdapter invoke(const FunctionsMap& funcsMap, const String& funcName,
                   const JsonAdapter& e);

/// No-Input function: a mask for ignoring input
template <class _callable>
inline JasInvokable __ni(_callable&& f) {
  return [f = std::move(f)](const JsonAdapter&) { return f(); };
}

/// Wrap object's method
template <class _Class, typename _Method>
inline JasInvokable __om(_Class* o, _Method method) {
  return std::bind(method, o, std::placeholders::_1);
}

/// context independent function
namespace cif {

std::vector<String> supportedFunctions();
bool supported(const String& funcName);
JsonAdapter invoke(const String& funcName, const JsonAdapter& e = {});

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
