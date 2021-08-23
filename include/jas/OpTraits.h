#pragma once

#include "EvaluableClasses.h"

namespace jas {
namespace op_traits {

template <class T, typename = bool>
struct OperatorsSupported;

using OperatorsSupportedBits = uint64_t;
inline constexpr OperatorsSupportedBits __SUPPORT_ALL =
    static_cast<OperatorsSupportedBits>(-1);
inline constexpr OperatorsSupportedBits __SUPPORT_NONE = 0;

template <class... _OpTypes>
inline constexpr OperatorsSupportedBits __supportOps(_OpTypes... ops) {
  return (
      (OperatorsSupportedBits{1} << static_cast<OperatorsSupportedBits>(ops)) |
      ...);
}
template <class _optype>
static constexpr OperatorsSupportedBits __2bits(_optype opt) {
  return OperatorsSupportedBits{1} << static_cast<OperatorsSupportedBits>(opt);
}

template <OperatorsSupportedBits AOT_SUPPORTED_OPS = __SUPPORT_ALL,
          OperatorsSupportedBits ASOT_SUPPORTED_OPS = __SUPPORT_ALL,
          OperatorsSupportedBits LOT_SUPPORTED_OPS = __SUPPORT_ALL,
          OperatorsSupportedBits COT_SUPPORTED_OPS = __SUPPORT_ALL>
struct __OSC {
  static constexpr bool supported(aot opt) {
    return AOT_SUPPORTED_OPS & __2bits(opt);
  }
  static constexpr bool supported(asot opt) {
    return ASOT_SUPPORTED_OPS & __2bits(opt);
  }
  static constexpr bool supported(lot opt) {
    return LOT_SUPPORTED_OPS & __2bits(opt);
  }
  static constexpr bool supported(cot opt) {
    return COT_SUPPORTED_OPS & __2bits(opt);
  }

  template <class OtherOperatorT>
  static constexpr bool supported(OtherOperatorT opt) {
    return false;
  }
};

auto constexpr __SP_AOT_ALL = __SUPPORT_ALL;
auto constexpr __SP_AOT_NONE = __SUPPORT_NONE;
auto constexpr __SP_ASOT_ALL = __SUPPORT_ALL;
auto constexpr __SP_ASOT_NONE = __SUPPORT_NONE;
auto constexpr __SP_LOT_ALL = __SUPPORT_ALL;
auto constexpr __SP_LOT_NONE = __SUPPORT_NONE;
auto constexpr __SP_COT_ALL = __SUPPORT_ALL;
auto constexpr __SP_COT_NONE = __SUPPORT_NONE;

template <>
struct OperatorsSupported<String>
    : __OSC<__supportOps(aot::plus), __supportOps(asot::s_plus), __SP_LOT_NONE,
            __SP_COT_ALL> {};

template <>
struct OperatorsSupported<bool>
    : __OSC<__SP_AOT_NONE,
            __supportOps(asot::s_bit_and, asot::s_bit_or, asot::s_bit_xor),
            __SP_LOT_ALL, __SP_COT_ALL> {};

template <typename T>
struct OperatorsSupported<T, std::enable_if_t<std::is_integral_v<T>, bool>>
    : __OSC<__SP_AOT_ALL, __SP_ASOT_ALL, __SP_LOT_ALL, __SP_COT_ALL> {};

template <typename T>
struct OperatorsSupported<T,
                          std::enable_if_t<std::is_floating_point_v<T>, bool>>
    : __OSC<__supportOps(aot::plus, aot::minus, aot::multiplies, aot::divides),
            __supportOps(asot::s_plus, asot::s_minus, asot::s_multiplies,
                         asot::s_divides),
            __SP_LOT_ALL, __SP_COT_ALL> {};

template <class T, typename>
struct OperatorsSupported
    : __OSC<__SP_AOT_NONE, __SP_ASOT_NONE, __SP_LOT_NONE, __SP_COT_NONE> {};

template <class T, class _OpType>
constexpr bool operationSupported(_OpType op) {
  return OperatorsSupported<T>::supported(op);
}

}  // namespace op_traits
}  // namespace jas
