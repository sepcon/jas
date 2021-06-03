#pragma once

#include <memory>
#include <vector>

#include "Json.h"

namespace jas {
class EvalContextIF;
using EvalContextPtr = std::shared_ptr<EvalContextIF>;
using EvalContexts = std::vector<EvalContextPtr>;

class EvalContextIF {
 public:
  virtual ~EvalContextIF() = default;
  virtual std::vector<String> supportedFunctions() const = 0;
  virtual bool functionSupported(const String& functionName) const = 0;
  virtual JsonAdapter invoke(const String& name, const JsonAdapter& param) = 0;
  virtual JsonAdapter property(const String& refid) const = 0;
  virtual void setProperty(const String& path, JsonAdapter val) = 0;
  virtual EvalContextPtr subContext(const String& ctxtID,
                                    const JsonAdapter& input) = 0;
  virtual String debugInfo() const = 0;
  //  virtual JsonAdapter data() const = 0;
};

}  // namespace jas
