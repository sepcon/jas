#pragma once

#include <memory>

#include "Json.h"
#include "String.h"

namespace jas {

class Evaluable;
using EvaluablePtr = std::shared_ptr<Evaluable>;

class Evaluable {
 public:
  virtual ~Evaluable() = default;
  virtual void accept(class EvaluatorBase* e) const = 0;
  virtual bool useStack() const = 0;
};

}  // namespace jas
