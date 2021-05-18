#pragma once

#include "Evaluable.h"
#include "Json.h"

namespace jas {
namespace parser {
EvaluablePtr parse(const Json& j);
};  // namespace parser
}  // namespace jas
