#pragma once

namespace jas {
namespace keyword {
// arithmetical operators
inline constexpr auto bit_and = "@bit_and";
inline constexpr auto bit_not = "@bit_not";
inline constexpr auto bit_or = "@bit_or";
inline constexpr auto bit_xor = "@bit_xor";
inline constexpr auto divides = "@divides";
inline constexpr auto minus = "@minus";
inline constexpr auto modulus = "@modulus";
inline constexpr auto multiplies = "@multiplies";
inline constexpr auto negate = "@negate";
inline constexpr auto plus = "@plus";
// logical operators
inline constexpr auto logical_and = "@and";
inline constexpr auto logical_not = "@not";
inline constexpr auto logical_or = "@or";
// comparison operators
inline constexpr auto eq = "@eq";
inline constexpr auto ge = "@ge";
inline constexpr auto gt = "@gt";
inline constexpr auto le = "@le";
inline constexpr auto lt = "@lt";
inline constexpr auto neq = "@neq";
// list operations
inline constexpr auto all_of = "@all_of";
inline constexpr auto any_of = "@any_of";
inline constexpr auto none_of = "@none_of";
inline constexpr auto size_of = "@size_of";
inline constexpr auto count_if = "@count_if";

inline constexpr auto cond = "@cond";
inline constexpr auto item_id = "@item_id";
inline constexpr auto list = "@list";
inline constexpr auto path = "@path";

// short functions
inline constexpr auto cmp_ver = "@cmp_ver";
inline constexpr auto prop = "@prop";
inline constexpr auto evchg = "@evchg";

inline constexpr auto call = "@call";
inline constexpr auto func = "@func";
inline constexpr auto value_of = "@value_of";
inline constexpr auto snapshot = "@snapshot";
inline constexpr auto param = "@param";

inline constexpr auto id = "#";

}  // namespace keyword

namespace prefix {
inline constexpr auto property = '#';
inline constexpr auto operation = '@';
}  // namespace prefix
}  // namespace jas
