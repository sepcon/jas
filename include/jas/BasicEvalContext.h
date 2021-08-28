#pragma once

#include <map>

#include "EvalContextIF.h"

namespace jas {

class BasicEvalContext : public EvalContextIF {
 public:
  using PropertyMap = std::map<String, Var>;

  BasicEvalContext(BasicEvalContext* parent = nullptr, String id = {});
  std::vector<String> supportedFunctions() const override;
  bool functionSupported(const StringView& functionName) const override;
  Var invoke(const String& name, const Var& param) override;
  Var* variable(const String& refid) override;
  Var* setVariable(const String& name, Var val) override;
  EvalContextPtr subContext(const String& ctxtID, const Var& input) override;
  Var findProperty(BasicEvalContext* ctxt, const String& name);
  String debugInfo() const override;
  const BasicEvalContext* rootContext() const;
  BasicEvalContext* rootContext();

 public:
  BasicEvalContext* parent_ = nullptr;
  String id_;
  PropertyMap variables_;
};

}  // namespace jas
