#pragma once

#include "String.h"

namespace jas {

struct ExceptionBase {
  ExceptionBase() = default;
  ExceptionBase(String d) : details{std::move(d)} {}
  virtual ~ExceptionBase() = default;
  virtual const String& what() const { return details; }
  String details;
};

#define __mc_jas_exception(name)                                       \
  struct name : public ExceptionBase {                                 \
    using _Base = ExceptionBase;                                       \
    name(const String& msg = {}) : _Base(strJoin(#name, ": ", msg)) {} \
  }

__mc_jas_exception(SyntaxError);
__mc_jas_exception(EvaluationError);

//#undef __mc_jas_exception

/// Exceptions
template <class _exception, class... _args>
void throwIf(bool cond, _args&&... args) {
  if (cond) {
    throw _exception{strJoin(std::forward<_args>(args)...)};
  }
}

template <class _exception, class... _args>
void throw_(_args&&... args) {
  _exception e{strJoin(std::forward<_args>(args)...)};
  throw e;
}

}  // namespace jas
