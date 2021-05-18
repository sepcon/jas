#pragma once

#include "Evaluable.h"
#include "String.h"

namespace jas {

class SyntaxValidator {
 public:
  SyntaxValidator();
  ~SyntaxValidator();

  bool validate(const Evaluable& e);
  bool validate(const EvaluablePtr& e);
  void clear();
  String generate_syntax(const Evaluable& e);
  String generate_syntax(const EvaluablePtr& e);
  String get_report();
  bool has_error() const;

 private:
  class SyntaxValidatorImpl* impl_ = nullptr;
};

}  // namespace jas
