#pragma once

#include "String.h"

namespace jas {
///
/// jas-1.0: First release to production
///     - Syntax Evaluator
///     - Historical evaluation context
///     - Translator from json to Evaluable
///     - Transform syntax to C/C++ style
///     - Built-in modules:
///         `- cif: context independent functions
///         `- alg: algorithms for sort/transform/...
///         `- list: functions for list data manipulation
///         `- dict: functions for dict data manipulation
/// jas-1.1: Some bug fixes
///     - Allow EvaluableList to be evaluated on stack, that we can transform input data to output as json-array
/// TODO:
///     1. Using FSM for translator,  replace for regular expression and chain-of-responsibility translators
///     2. Add branching/loop
/// CURRENT-VERSION: 1.1

inline const auto version = JASSTR("1.1");
inline const auto version_expression_key = JASSTR("$jas_version");
}  // namespace jas
