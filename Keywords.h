#pragma once
#include "String.h"

namespace jas {
namespace keyword {
// arithmetical operators
inline const auto bit_and = JASSTR("@bit_and");
inline const auto bit_not = JASSTR("@bit_not");
inline const auto bit_or = JASSTR("@bit_or");
inline const auto bit_xor = JASSTR("@bit_xor");
inline const auto divides = JASSTR("@divides");
inline const auto minus = JASSTR("@minus");
inline const auto modulus = JASSTR("@modulus");
inline const auto multiplies = JASSTR("@multiplies");
inline const auto negate = JASSTR("@negate");
inline const auto plus = JASSTR("@plus");
// logical operators
inline const auto logical_and = JASSTR("@and");
inline const auto logical_not = JASSTR("@not");
inline const auto logical_or = JASSTR("@or");
// comparison operators
inline const auto eq = JASSTR("@eq");
inline const auto ge = JASSTR("@ge");
inline const auto gt = JASSTR("@gt");
inline const auto le = JASSTR("@le");
inline const auto lt = JASSTR("@lt");
inline const auto neq = JASSTR("@neq");
// list operations
inline const auto all_of = JASSTR("@all_of");
inline const auto any_of = JASSTR("@any_of");
inline const auto none_of = JASSTR("@none_of");
inline const auto size_of = JASSTR("@size_of");
inline const auto count_if = JASSTR("@count_if");
inline const auto filter_if = JASSTR("@filter_if");
inline const auto transform = JASSTR("@transform");

// field specifiers
inline const auto paste = JASSTR("@paste");
inline const auto cond = JASSTR("@cond");
inline const auto op = JASSTR("@op");
inline const auto list = JASSTR("@list");
inline const auto id = JASSTR("$");

}  // namespace keyword

namespace prefix {
inline const auto variable = JASSTR('$');
inline const auto specifier = JASSTR('@');
}  // namespace prefix
}  // namespace jas
