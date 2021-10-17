/// Context Independent Functions
#pragma once

#include "Var.h"

namespace jas {
namespace mdl {
namespace cif {

Var current_time();
Var current_time_diff(const Var&);
Var tolower(const Var&);
Var toupper(const Var&);
Var cmp_ver(const Var&);
Var eq_ver(const Var&);
Var ne_ver(const Var&);
Var lt_ver(const Var&);
Var gt_ver(const Var&);
Var ge_ver(const Var&);
Var le_ver(const Var&);
Var match_ver(const Var&);
Var contains(const Var&);
Var to_string(const Var&);
Var unix_timestamp(const Var&);
Var has_null_val(const Var&);
Var len(const Var&);
Var is_even(const Var&);
Var is_odd(const Var&);
Var empty(const Var&);
Var not_empty(const Var&);
Var abs(const Var&);

}  // namespace cif
}  // namespace mdl
}  // namespace jas
