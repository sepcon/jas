#include "Builtin.ListOps.h"

#include "jas/CIF.h"
#include "jas/SyntaxEvaluatorImpl.h"

namespace jas {
namespace builtin {
namespace list {

using ModuleFunction = JsonAdapterPtr (*)(const JsonAdapterPtr &thelist,
                                          const JsonAdapterPtr &params);
using FuncMap = std::map<StringView, ModuleFunction>;

#define __throw_if_param_null(param)         \
  __jas_ni_func_throw_invalidargs_if(!param, \
                                     "List function param must not be null");

#define __list_throw_if_not_array(thelist)                                     \
  __jas_ni_func_throw_invalidargs_if(!thelist, "The list must  not be null "); \
  __jas_func_throw_invalidargs_if(!JsonTrait::isArray(thelist->value),         \
                                  "The list must be array", *thelist)

struct ListOperationModule : public FunctionModuleIF {
  JsonAdapterPtr eval(const FunctionInvocation &fi,
                      SyntaxEvaluatorImpl *evaluator) override {
    assert(supported(fi.name));
    return {};
    //    auto evalParam = evaluator->_evalRet(fi.param.get());
  }
};

JsonAdapterPtr append(const JsonAdapterPtr &thelist,
                      const JsonAdapterPtr &params) {
  __list_throw_if_not_array(thelist);
  __throw_if_param_null(params);

  if (params->isArray()) {
    JsonTrait::iterateArray(params->value, [&](auto &&elem) {
      JsonTrait::add(*thelist, elem);
      return true;
    });
  }
  return thelist;
}

static const FuncMap &_funcMap() {
  const static FuncMap _ = {
      {JASSTR("append"), append},
  };
  return _;
}

const CharType *moduleName() { return "list"; }

bool supported(const StringView &func) {
  return _funcMap().find(func) != std::end(_funcMap());
}

void listFuncs(std::vector<String> &funcs) {
  for (auto &[name, _] : _funcMap()) {
    funcs.push_back(String{name});
  }
}

JsonAdapterPtr invoke(const String &funcname, const JsonAdapterPtr &thelist,
                      const JsonAdapterPtr &params) {
  return {};
}

FunctionModuleIF *makeModule() { return new ListOperationModule; }

}  // namespace list
}  // namespace builtin
}  // namespace jas
