#pragma once

#include <set>

#include "EvalContextIF.h"
#include "Evaluable.h"

namespace jas {
using ContextPtr = std::shared_ptr<class EvalContextIF>;
class ModuleManager;
struct TranslatorImpl;

class Translator {
 public:
  enum class Strategy {
    AllowShorthand,
    Formal,
  };

  Translator(ModuleManager* moduleMgr);
  ~Translator();
  EvaluablePtr translate(EvalContextPtr ctxt, const Var& jas,
                         Strategy strategy = Strategy::AllowShorthand);
  Var reconstructJAS(EvalContextPtr ctxt, const Var& script);
  static const std::set<StringView>& evaluableSpecifiers();

  TranslatorImpl* impl_ = nullptr;
};  // namespace parser
}  // namespace jas
