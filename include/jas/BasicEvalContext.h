#pragma once

#include <map>

#include "EvalContextIF.h"

namespace jas {

class BasicEvalContext : public EvalContextIF {
 public:
  using VariableMap = std::map<String, Var>;

  BasicEvalContext(BasicEvalContext *parent = nullptr, String id = {},
                   ContextArguments input = {});
  // EvalContextIF interface
 public:
  std::vector<String> supportedFunctions() const override;
  bool functionSupported(const StringView &functionName) const override;
  Var invoke(const String &name, const Var &param) override;
  Var *lookupVariable(const String &name) override;
  Var *putVariable(const String &name, Var val) override;
  EvalContextPtr subContext(const String &ctxtID,
                            ContextArguments input) override;
  Var arg(uint8_t pos) const noexcept override;
  void args(ContextArguments args) override;
  const ContextArguments &args() const noexcept override;
  String debugInfo() const override;

  const BasicEvalContext *rootContext() const;
  BasicEvalContext *rootContext();

 protected:
  BasicEvalContext *parent_ = nullptr;
  String id_;
  VariableMap variables_;
  ContextArguments args_;
};

}  // namespace jas
