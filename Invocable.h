#pragma once

#include "EvaluableClasses.h"
#include "Exception.h"

namespace jas {

using JasInvokable = std::function<DirectVal(const DirectVal&)>;
using FunctionsMap = std::map<String, JasInvokable>;
__mc_jas_exception(function_not_found_error);
__mc_jas_exception(function_not_implemented_error);

DirectVal invokeFunction(const FunctionsMap& funcs_map, const String& func_name,
                         const DirectVal& e);
DirectVal globalInvoke(const String& funcName, const DirectVal& e = {});
void errorNotImplemented(const String& funcName);

// No-Input function: a mask for ignoring input
template <class _callable>
inline JasInvokable __ni(_callable&& f) {
  return [f = std::move(f)](const DirectVal&) { return f(); };
}

template <class _Class, typename _Method>
inline JasInvokable __om(_Class* o, _Method method) {
  return std::bind(method, o, std::placeholders::_1);
}

}  // namespace jas
