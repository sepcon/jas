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
  static String syntaxOf(const Evaluable& e);
  static String syntaxOf(const EvaluablePtr& e);

 private:
  class SyntaxValidatorImpl* impl_ = nullptr;
};

}  // namespace jas
