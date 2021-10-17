#pragma once

#include "String.h"

namespace jas {

struct Exception {
  Exception() = default;
  Exception(String d) : details{std::move(d)} {}
  virtual ~Exception() = default;
  virtual const String& what() const { return details; }
  String details;
};

#define __mc_jas_exception(name)                                       \
  struct name : public jas::Exception {                                \
    using _Base = jas::Exception;                                      \
    name(const String& msg = {}) : _Base(strJoin(#name, ": ", msg)) {} \
  }

__mc_jas_exception(SyntaxError);
__mc_jas_exception(EvaluationError);
__mc_jas_exception(TypeError);
__mc_jas_exception(InvalidArgument);
__mc_jas_exception(OutOfRange);

//#undef __mc_jas_exception

template <class _exception, class... _args>
void throw_(_args&&... args) {
  _exception e{strJoin(std::forward<_args>(args)...)};
  throw e;
}

#define __jas_throw(ExceptionType, ...) \
  throw_<ExceptionType>(__VA_ARGS__);   \
  throw  // avoid throw
#define __jas_throw_if(ExceptionType, condition, ...) \
  if (condition) {                                    \
    __jas_throw(ExceptionType, __VA_ARGS__);          \
  }

}  // namespace jas
