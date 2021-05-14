#pragma once

#include "jas_string.h"

namespace jas {

struct exception_base {
  exception_base() = default;
  exception_base(string_type d) : details{std::move(d)} {}
  virtual ~exception_base() = default;
  virtual const string_type& what() const { return details; }
  string_type details;
};

#define __mc_jas_exception(name)        \
  struct name : public exception_base { \
    using _base = exception_base;       \
    using _base::_base;                 \
  }

__mc_jas_exception(syntax_error);
__mc_jas_exception(evaluation_error);
__mc_jas_exception(param_mismatch_error);
__mc_jas_exception(invalid_op_error);

#undef __mc_jas_exception

/// Exceptions
template <class _exception, class... _args>
void __throw_if(bool cond, _args&&... args) {
  if (cond) {
    throw _exception{std::forward<_args>(args)...};
  }
}

template <class _exception, class... _args>
void __throw(_args&&... args) {
  _exception e{std::forward<_args>(args)...};
  throw e;
}

}  // namespace jas
