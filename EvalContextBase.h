#pragma once

#include <map>

#include "EvalContextIF.h"

namespace jas {

class EvalContextBase : public EvalContextIF {
 public:
  using PropertyMap = std::map<String, Json>;

  EvalContextBase(EvalContextBase* parent = nullptr, String id = {});
  std::vector<String> supportedFunctions() const override;
  bool functionSupported(const String& functionName) const override;
  JsonAdapter invoke(const String& name, const JsonAdapter& param) override;
  JsonAdapter property(const String& refid) const override;
  void setProperty(const String& path, JsonAdapter val) override;
  String debugInfo() const override;
  const EvalContextBase* rootContext() const;
  EvalContextBase* rootContext();

 public:
  EvalContextBase* parent_ = nullptr;
  String id_;
  PropertyMap properties_;
};

}  // namespace jas
