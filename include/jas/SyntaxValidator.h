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
  String generateSyntax(const Evaluable& e);
  String generateSyntax(const EvaluablePtr& e);
  String getReport();
  bool hasError() const;

 private:
  class SyntaxValidatorImpl* impl_ = nullptr;
};

}  // namespace jas
