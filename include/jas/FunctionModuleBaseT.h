#pragma once

#include "FunctionModule.h"

namespace jas {

template <class FunctionInvocationType>
class FunctionModuleBaseT : public FunctionModuleIF {
 public:
  using FunctionsMap = std::map<String, FunctionInvocationType, std::less<>>;
  using FunctionInvoker = Var (*)(const FunctionInvocationType&,
                                  const String& funcName, EvaluablePtr param,
                                  SyntaxEvaluatorImpl* evaluator);

  Var eval(const String& funcName, EvaluablePtr param,
           SyntaxEvaluatorImpl* evaluator) override {
    auto it = _funcMap().find(funcName);

    __jas_throw_if(FunctionNotFoundError, it == std::end(_funcMap()),
                   "Unknown reference to function `", moduleName(), ".",
                   funcName, "`");
    return this->invoke(it->second, std::move(param), evaluator);
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
  virtual Var invoke(const FunctionInvocationType&, EvaluablePtr param,
                     SyntaxEvaluatorImpl* evaluator) = 0;
  virtual const FunctionsMap& _funcMap() const = 0;
};

}  // namespace jas
