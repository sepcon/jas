#pragma once

#include "Exception.h"
#include "Json.h"

namespace jas {
namespace jaop {

using std::move;
inline void __arthThrowIf(bool cond, const String& msg,
                          const JsonAdapter& first, const JsonAdapter& second) {
  throwIf<EvaluationError>(cond, msg, ": (", first.dump(), ", ", second.dump(),
                           ")");
}

inline void __throwIfNotAllNumbers(const JsonAdapter& first,
                                   const JsonAdapter& second) {
  __arthThrowIf(!(first.isNumber() && second.isNumber()),
                JASSTR("Expect 2 numbers"), first, second);
}

template <class _StdOp>
struct __Applier {
  __Applier(const char* name, const JsonAdapter& first,
            const JsonAdapter& second)
      : first_(first), second_(second), name_(name) {}

  template <class _Type>
  __Applier& applyAs() {
    if (first_.isType<_Type>()) {
      applied_ = true;
      __arthThrowIf(
          !second_.isType<_Type>(),
          strJoin("operator[", name_, "] Expected 2 values of same type"),
          first_, second_);
      result_ = _StdOp{}(first_.get<_Type>(), second_.get<_Type>());
    }
    return *this;
  }

  __Applier& applyAsNumber() {
    if (first_.isNumber()) {
      applied_ = true;
      __arthThrowIf(!second_.isNumber(),
                    strJoin("operator[", name_, "] Expected 2 numbers"), first_,
                    second_);
      result_ = _StdOp{}(first_.get<double>(), second_.get<double>());
    }
    return *this;
  }

  JsonAdapter takeResult() {
    __arthThrowIf(!applied_, strJoin("operator[", name_, "] Not applicable on"),
                  first_, second_);
    return move(result_);
  }

  operator bool() const { return applied_; }

  const JsonAdapter& first_;
  const JsonAdapter& second_;
  JsonAdapter result_;
  const char* name_;
  bool applied_ = false;
};

template <class _StdOp>
auto __applier(const char* name, const JsonAdapter& first,
               const JsonAdapter& second) {
  return __Applier<_StdOp>(name, first, second);
}

inline JsonAdapter operator+(const JsonAdapter& first,
                             const JsonAdapter& second) {
  auto applier = __applier<std::plus<>>("+", first, second);
  applier.applyAsNumber() || applier.applyAs<String>();
  return applier.takeResult();
}

inline JsonAdapter operator-(const JsonAdapter& first,
                             const JsonAdapter& second) {
  return __applier<std::minus<>>("-", first, second)
      .applyAsNumber()
      .takeResult();
}

inline JsonAdapter operator*(const JsonAdapter& first,
                             const JsonAdapter& second) {
  return __applier<std::multiplies<>>("*", first, second)
      .applyAsNumber()
      .takeResult();
}

inline JsonAdapter operator/(const JsonAdapter& first,
                             const JsonAdapter& second) {
  __throwIfNotAllNumbers(first, second);
  __arthThrowIf(std::numeric_limits<double>::epsilon() >
                    std::abs(second.get<double>() - 0.0),
                JASSTR("Devide by zero"), first, second);
  return first.get<double>() / second.get<double>();
}
inline JsonAdapter operator<(const JsonAdapter& first,
                             const JsonAdapter& second) {
  auto applier = __applier<std::less<>>("<", first, second);
  applier.applyAsNumber() || applier.applyAs<String>();
  return applier.takeResult();
}

inline bool operator>(const JsonAdapter& first, const JsonAdapter& second) {
  return second < first;
}
inline bool operator==(const JsonAdapter& first, const JsonAdapter& second) {
  return JsonTrait::equal(second, first);
}
inline bool operator!=(const JsonAdapter& first, const JsonAdapter& second) {
  return !JsonTrait::equal(second, first);
}
inline bool operator>=(const JsonAdapter& first, const JsonAdapter& second) {
  return JsonTrait::equal(second, first) || first > second;
}
inline bool operator<=(const JsonAdapter& first, const JsonAdapter& second) {
  return JsonTrait::equal(second, first) || first < second;
}

}  // namespace jaop
using jaop::operator!=;
using jaop::operator==;
using jaop::operator>;
using jaop::operator>=;
using jaop::operator<;
using jaop::operator<=;
using jaop::operator+;
using jaop::operator-;
using jaop::operator*;
using jaop::operator/;

}  // namespace jas
