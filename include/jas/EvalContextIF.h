#pragma once

#include <memory>
#include <vector>

#include "Json.h"
#include "Var.h"

namespace jas {
class EvalContextIF;
using EvalContextPtr = std::shared_ptr<EvalContextIF>;
using EvalContexts = std::vector<EvalContextPtr>;
using ContextArguments = Var::List;

class EvalContextIF {
 public:
  virtual ~EvalContextIF() = default;
  virtual std::vector<String> supportedFunctions() const = 0;
  virtual bool functionSupported(const StringView& functionName) const = 0;
  virtual Var invoke(const String& name, const Var& param) = 0;
  virtual Var* lookupVariable(const String& name) = 0;
  virtual Var* putVariable(const String& name, Var val) = 0;
  virtual EvalContextPtr subContext(const String& ctxtID,
                                    ContextArguments input) = 0;
  virtual Var arg(uint8_t pos) const noexcept = 0;
  virtual void args(ContextArguments) = 0;
  virtual const ContextArguments& args() const noexcept = 0;
  virtual String debugInfo() const = 0;
};

}  // namespace jas
