#pragma once

#include "jas/EvaluableClasses.h"
#include "jas/FunctionModule.h"
#include "jas/Json.h"

namespace jas {
namespace builtin {
namespace list {

using JsonAdapterPtr = std::shared_ptr<JsonAdapter>;
const CharType* moduleName();
void listFuncs(std::vector<String>&);
bool supported(const StringView& name);
JsonAdapterPtr invoke(const String& funcname, const JsonAdapterPtr& thelist,
                      const JsonAdapterPtr& params);
FunctionModuleIF* makeModule();

}  // namespace list
}  // namespace builtin
}  // namespace jas
