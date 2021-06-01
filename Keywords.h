#pragma once
#include <string_view>

namespace jas {
namespace keyword {
using namespace std::string_view_literals;
// arithmetical operators
inline constexpr auto bit_and = "@bit_and"sv;
inline constexpr auto bit_not = "@bit_not"sv;
inline constexpr auto bit_or = "@bit_or"sv;
inline constexpr auto bit_xor = "@bit_xor"sv;
inline constexpr auto divides = "@divides"sv;
inline constexpr auto minus = "@minus"sv;
inline constexpr auto modulus = "@modulus"sv;
inline constexpr auto multiplies = "@multiplies"sv;
inline constexpr auto negate = "@negate"sv;
inline constexpr auto plus = "@plus"sv;
// logical operators
inline constexpr auto logical_and = "@and"sv;
inline constexpr auto logical_not = "@not"sv;
inline constexpr auto logical_or = "@or"sv;
// comparison operators
inline constexpr auto eq = "@eq"sv;
inline constexpr auto ge = "@ge"sv;
inline constexpr auto gt = "@gt"sv;
inline constexpr auto le = "@le"sv;
inline constexpr auto lt = "@lt"sv;
inline constexpr auto neq = "@neq"sv;
// list operations
inline constexpr auto all_of = "@all_of"sv;
inline constexpr auto any_of = "@any_of"sv;
inline constexpr auto none_of = "@none_of"sv;
inline constexpr auto size_of = "@size_of"sv;
inline constexpr auto count_if = "@count_if"sv;

// short functions
inline constexpr auto cmp_ver = "@cmp_ver"sv;
inline constexpr auto prop = "@prop"sv;
inline constexpr auto evchg = "@evchg"sv;

inline constexpr auto call = "@call"sv;
inline constexpr auto value_of = "@value_of"sv;

// field specifiers
inline constexpr auto func = "@func"sv;
inline constexpr auto snapshot = "@snapshot"sv;
inline constexpr auto param = "@param"sv;
inline constexpr auto cond = "@cond"sv;
inline constexpr auto item_id = "@item_id"sv;
inline constexpr auto list = "@list"sv;
inline constexpr auto path = "@path"sv;

inline constexpr auto id = "#"sv;

}  // namespace keyword

namespace prefix {
inline constexpr auto property = '#';
inline constexpr auto specifier = '@';
}  // namespace prefix
}  // namespace jas
