#pragma once

namespace jas {

class Evaluable;
struct Constant;
struct EvaluableDict;
struct EvaluableList;
struct ArithmaticalOperator;
struct ArthmSelfAssignOperator;
struct LogicalOperator;
struct ComparisonOperator;
struct ListAlgorithm;
struct ModuleFI;
struct ContextFI;
struct MacroFI;
struct EvaluatorFI;
struct ObjectPropertyQuery;
struct Variable;
struct ContextArgument;
struct ContextArgumentsInfo;

class EvaluatorIF {
 public:
  virtual ~EvaluatorIF() = default;
  virtual void eval(const Constant&) = 0;
  virtual void eval(const EvaluableDict&) = 0;
  virtual void eval(const EvaluableList&) = 0;
  virtual void eval(const ArithmaticalOperator&) = 0;
  virtual void eval(const ArthmSelfAssignOperator&) = 0;
  virtual void eval(const LogicalOperator&) = 0;
  virtual void eval(const ComparisonOperator&) = 0;
  virtual void eval(const ListAlgorithm&) = 0;
  virtual void eval(const ModuleFI&) = 0;
  virtual void eval(const MacroFI&) = 0;
  virtual void eval(const ContextFI&) = 0;
  virtual void eval(const EvaluatorFI&) = 0;
  virtual void eval(const ObjectPropertyQuery&) = 0;
  virtual void eval(const Variable&) = 0;
  virtual void eval(const ContextArgument&) = 0;
  virtual void eval(const ContextArgumentsInfo&) = 0;
};

}  // namespace jas
