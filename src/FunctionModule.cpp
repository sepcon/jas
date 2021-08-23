#include "jas/FunctionModule.h"

namespace jas {
using std::move;
using ModuleMap = std::map<String, FunctionModulePtr>;

static ModuleMap &_modules() {
  static ModuleMap _ = {};
  return _;
}

bool registerModule(const String &moduleName, FunctionModulePtr mdl) {
  auto &tobePut = _modules()[moduleName];
  if (!tobePut) {
    tobePut = move(mdl);
    return true;
  }
  return false;
}

bool unregisterModule(const String &moduleName) {
  return _modules().erase(moduleName) != 0;
}

JsonAdapterPtr eval(const FunctionInvocation &fi, SyntaxEvaluatorImpl *) {
  return {};
}

}  // namespace jas
