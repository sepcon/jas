#pragma once

#include <memory>

#include "Json.h"
#include "String.h"

namespace jas {

class Macro;
class Evaluable;
using EvaluablePtr = std::shared_ptr<Evaluable>;
using MacroPtr = std::shared_ptr<Macro>;

class Evaluable {
 public:
  Evaluable(Evaluable* parent = nullptr) : parent(parent) {}
  virtual ~Evaluable() = default;
  virtual void accept(class EvaluatorIF* e) const = 0;
  virtual bool useStack() const = 0;
  virtual MacroPtr resolveMacro(const StringView& name) {
    if (parent) {
      return parent->resolveMacro(name);
    } else {
      return {};
    }
  }
  Evaluable* parent = nullptr;
};

}  // namespace jas
