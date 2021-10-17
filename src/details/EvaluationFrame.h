#pragma once

#include <map>
#include <memory>

#include "jas/EvalContextIF.h"
#include "jas/Evaluable.h"
#include "jas/String.h"

namespace jas {

enum class VariableStatus;
class EvaluationFrame;
using EvaluationFramePtr = std::shared_ptr<EvaluationFrame>;
using VariableStatusMap = std::map<String, VariableStatus>;
using VariableStatusMapPtr = std::shared_ptr<VariableStatusMap>;

enum class VariableStatus {
  Undefined,
  NotEvaluated,
  Evaluating,
  Evaluated,
};

class EvaluationFrame {
 public:
  EvaluationFrame(EvaluationFramePtr parent, EvalContextPtr _context = {},
                  Var _returnedValue = {}, const Evaluable* _evb = nullptr)
      : parent(std::move(parent)),
        context(_context),
        returnedValue(_returnedValue),
        evb(_evb) {}

  EvaluationFramePtr parent;
  EvalContextPtr context;
  Var returnedValue;
  const Evaluable* evb;
  VariableStatusMapPtr variableStatusMapPtr;

  VariableStatus variableStatus(const String& varname) const {
    if (variableStatusMapPtr) {
      auto it = variableStatusMapPtr->find(varname);
      return it != std::end(*variableStatusMapPtr) ? it->second
                                                   : VariableStatus::Undefined;
    }
    return VariableStatus::Undefined;
  }
  void finishEvaluatingVar(const String& varname) {
    assert(variableStatusMapPtr);
    (*variableStatusMapPtr)[varname] = VariableStatus::Evaluated;
  }
  void startEvaluatingVar(const String& varname) {
    assert(variableStatusMapPtr);
    (*variableStatusMapPtr)[varname] = VariableStatus::Evaluating;
  }
};
}  // namespace jas
