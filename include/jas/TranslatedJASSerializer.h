#pragma once

#include "Evaluable.h"

namespace jas {

class Evaluable;
namespace serialization {

bool serialize(std::ostream& os, const Evaluable* evb);
EvaluablePtr deserialize(std::istream& is);

};  // namespace serialization
}  // namespace jas
