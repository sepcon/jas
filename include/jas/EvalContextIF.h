#pragma once

#include <memory>
#include <vector>

#include "Json.h"
#include "Var.h"

namespace jas {
class EvalContextIF;
using EvalContextPtr = std::shared_ptr<EvalContextIF>;
using EvalContexts = std::vector<EvalContextPtr>;

class EvalContextIF {
 public:
  virtual ~EvalContextIF() = default;
  virtual std::vector<String> supportedFunctions() const = 0;
  virtual bool functionSupported(const StringView& functionName) const = 0;
  virtual Var invoke(const String& name, const Var& param) = 0;
  virtual Var* variable(const String& name) = 0;
  virtual Var* setVariable(const String& name, Var val) = 0;
  virtual EvalContextPtr subContext(const String& ctxtID, const Var& input) = 0;
  virtual String debugInfo() const = 0;
};

}  // namespace jas
