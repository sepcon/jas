#pragma once

namespace jas {
struct Number {
  using UnderType = double;

  // Number(bool) = delete;
  template <class T, std::enable_if_t<std::is_floating_point_v<T> ||
                                          std::is_integral_v<T>,
                                      bool> = true>
  Number(T v) : value(static_cast<UnderType>(v)) {}
  Number() = default;
  Number(const Number&) = default;
  Number& operator=(const Number&) = default;

  bool isUnsigned() const;
  bool isInt() const;
  bool isDouble() const;
  Number& operator+=(const Number& other);
  Number& operator-=(const Number& other);
  Number& operator*=(const Number& other);
  Number& operator/=(const Number& other);
  Number& operator%=(const Number& other);

  template <class T>
  operator T() const {
    if constexpr (std::is_integral_v<T>) {
      return static_cast<T>(std::trunc(value));
    } else if constexpr (std::is_floating_point_v<T>) {
      return static_cast<T>(value);
    } else {
      static_assert(false, "Invalid conversion");
    }
  }

  UnderType value = 0.0;
};

bool operator==(const Number& lhs, const Number& rhs);
bool operator!=(const Number& lhs, const Number& rhs);
bool operator<(const Number& lhs, const Number& rhs);
bool operator>(const Number& lhs, const Number& rhs);
bool operator<=(const Number& lhs, const Number& rhs);
bool operator>=(const Number& lhs, const Number& rhs);

Number operator+(const Number& lhs, const Number& rhs);
Number operator-(const Number& lhs, const Number& rhs);
Number operator*(const Number& lhs, const Number& rhs);
Number operator/(const Number& lhs, const Number& rhs);
Number operator%(const Number& lhs, const Number& rhs);
Number operator<<(const Number& lhs, const Number& rhs);
Number operator>>(const Number& lhs, const Number& rhs);
Number operator|(const Number& lhs, const Number& rhs);
Number operator&(const Number& lhs, const Number& rhs);
Number operator^(const Number& lhs, const Number& rhs);
Number operator~(const Number& lhs);
Number operator-(const Number& lhs);

}  // namespace jas
