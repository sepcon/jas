#include "jas/Number.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "jas/Exception.h"

namespace jas {

using IntType = int64_t;

#define __number_req_all_integers(first, second)                      \
  __jas_throw_if(InvalidArgument, !(first.isInt() && second.isInt()), \
                 strJoin(__FUNCTION__, " require all integers"))

#define __number_req_integer(val)                 \
  __jas_throw_if(InvalidArgument, !(val.isInt()), \
                 strJoin(__FUNCTION__, " require an integer"))

Number &Number::operator%=(const Number &other) {
  __jas_throw_if(InvalidArgument, !this->isInt() || !other.isInt(),
                 "% applied only on integer");
  value = static_cast<UnderlyingType>(static_cast<IntType>(*this) %
                                      static_cast<IntType>(other));
  return *this;
}

Number &Number::operator/=(const Number &other) {
  __jas_throw_if(InvalidArgument, other == Number{0}, "Divide by zero");
  this->value /= other.value;
  return *this;
}

Number &Number::operator*=(const Number &other) {
  this->value *= other.value;
  return *this;
}

Number &Number::operator-=(const Number &other) {
  this->value -= other.value;
  return *this;
}

Number &Number::operator+=(const Number &other) {
  this->value += other.value;
  return *this;
}

bool Number::isDouble() const { return !isInt(); }

bool Number::isUnsigned() const { return value < 0; }

bool Number::isInt() const { return std::trunc(value) == value; }

Number operator%(const Number &lhs, const Number &rhs) {
  __number_req_all_integers(rhs, lhs);
  return static_cast<IntType>(lhs) % static_cast<IntType>(rhs);
}

Number operator/(const Number &lhs, const Number &rhs) {
  __jas_throw_if(InvalidArgument, rhs == Number{0}, "Divide by zero");
  return lhs.value / rhs.value;
}

Number operator*(const Number &lhs, const Number &rhs) {
  return lhs.value * rhs.value;
}

Number operator-(const Number &lhs, const Number &rhs) {
  return lhs.value - rhs.value;
}

Number operator+(const Number &lhs, const Number &rhs) {
  return lhs.value + rhs.value;
}

bool operator>=(const Number &lhs, const Number &rhs) {
  return lhs.value >= rhs.value;
}

bool operator<=(const Number &lhs, const Number &rhs) {
  return lhs.value <= rhs.value;
}

bool operator>(const Number &lhs, const Number &rhs) {
  return lhs.value > rhs.value;
}

bool operator<(const Number &lhs, const Number &rhs) {
  return lhs.value < rhs.value;
}

bool operator!=(const Number &lhs, const Number &rhs) {
  return !operator==(lhs, rhs);
}

bool operator==(const Number &lhs, const Number &rhs) {
  return lhs.value == rhs.value;
}
Number operator<<(const Number &lhs, const Number &rhs) {
  __number_req_all_integers(rhs, lhs);
  return static_cast<IntType>(lhs) << static_cast<IntType>(rhs);
}
Number operator>>(const Number &lhs, const Number &rhs) {
  __number_req_all_integers(rhs, lhs);
  return static_cast<IntType>(lhs) >> static_cast<IntType>(rhs);
}
Number operator|(const Number &lhs, const Number &rhs) {
  __number_req_all_integers(rhs, lhs);
  return static_cast<IntType>(lhs) | static_cast<IntType>(rhs);
}
Number operator&(const Number &lhs, const Number &rhs) {
  __number_req_all_integers(rhs, lhs);
  return static_cast<IntType>(lhs) & static_cast<IntType>(rhs);
}
Number operator^(const Number &lhs, const Number &rhs) {
  __number_req_all_integers(rhs, lhs);
  return static_cast<IntType>(lhs) ^ static_cast<IntType>(rhs);
}
Number operator~(const Number &lhs) {
  __number_req_integer(lhs);
  return ~static_cast<IntType>(lhs);
}

Number operator-(const Number &lhs) { return -lhs.value; }

}  // namespace jas
