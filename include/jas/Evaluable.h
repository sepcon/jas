#pragma once

#include <memory>

#include "Json.h"
#include "String.h"

namespace jas {

struct Evaluable;
using EvaluablePtr = std::shared_ptr<Evaluable>;

struct Evaluable {
  Evaluable(String _id = {}) : id(std::move(_id)) {}
  virtual ~Evaluable() = default;
  virtual void accept(class EvaluatorBase* e) const = 0;
  virtual bool useStack() const = 0;
  String id;
};

}  // namespace jas