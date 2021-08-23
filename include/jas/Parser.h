#pragma once

#include <set>

#include "EvalContextIF.h"
#include "Evaluable.h"

namespace jas {
using ContextPtr = std::shared_ptr<class EvalContextIF>;
namespace parser {
enum class Strategy {
  AllowShorthand,
  Formal,
};

EvaluablePtr parse(EvalContextPtr ctxt, const Json& jas,
                   Strategy strategy = Strategy::AllowShorthand);
Json reconstructJAS(EvalContextPtr ctxt, const Json& jas);

const std::set<StringView> &evaluableSpecifiers();

};  // namespace parser
}  // namespace jas
