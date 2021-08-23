#pragma once

#include <map>

#include "EvalContextIF.h"

namespace jas {

class EvalContextStandard : public EvalContextIF {
 public:
  using PropertyMap = std::map<String, JAdapterPtr>;

  EvalContextStandard(EvalContextStandard* parent = nullptr, String id = {});
  bool functionSupported(const StringView& functionName) const override;
  JsonAdapter invoke(const String& name, const JsonAdapter& param) override;
  JAdapterPtr property(const String& refid) const override;
  void setProperty(const String& name, JAdapterPtr val) override;
  JAdapterPtr* findProperty(EvalContextStandard* ctxt, const String& name);
  String debugInfo() const override;
  const EvalContextStandard* rootContext() const;
  EvalContextStandard* rootContext();

 public:
  EvalContextStandard* parent_ = nullptr;
  String id_;
  PropertyMap properties_;
};

}  // namespace jas
