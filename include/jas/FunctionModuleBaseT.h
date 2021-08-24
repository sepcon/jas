#pragma once

#include "FunctionModule.h"

namespace jas {

template <class FunctionInvocationType>
class FunctionModuleBaseT : public FunctionModuleIF {
 public:
  using FunctionsMap = std::map<String, FunctionInvocationType, std::less<>>;

  Var eval(const String& funcName, Var& evaluatedParam,
              SyntaxEvaluatorImpl*) override {
    auto it = _funcMap().find(funcName);

    throwIf<FunctionNotFoundError>(it == std::end(_funcMap()),
                                   "Unknown reference to function `",
                                   moduleName(), ".", funcName, "`");
    return it->second(evaluatedParam);
  }

  bool has(const StringView& funcName) const override {
    return _funcMap().find(funcName) != std::end(_funcMap());
  }

  void enumerateFuncs(FunctionNameList& list) const override {
    auto mname = moduleName();
    for (auto& [func, _] : _funcMap()) {
      if (!mname.empty()) {
        list.push_back(strJoin(moduleName(), '.', func));
      } else {
        list.push_back(func);
      }
    }
  }

 protected:
  virtual const FunctionsMap& _funcMap() const = 0;
};

}  // namespace jas
