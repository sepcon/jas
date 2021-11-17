#pragma once

#include <map>
#include <memory>
#include <vector>

#include "Evaluable.h"
#include "EvaluableClassesFwd.h"
#include "Exception.h"
#include "FunctionModule.h"
#include "Json.h"
#include "String.h"
#include "Var.h"

namespace jas {

struct VariableEvalInfo;
using Evaluables = std::vector<EvaluablePtr>;
using LocalVariables = std::map<String, VariableEvalInfo, std::less<>>;
using LocalVariablesPtr = std::shared_ptr<LocalVariables>;
using LocalMacrosMap = std::map<String, MacroPtr, std::less<>>;
using LocalMacrosMapPtr = std::shared_ptr<LocalMacrosMap>;

struct VariableEvalInfo {
  enum Type { Declaration, Update };
  VariableEvalInfo(EvaluablePtr v, Type tp = Declaration)
      : value(std::move(v)), type(tp) {}

  EvaluablePtr value;
  Type type;
};

class Macro {
 public:
  EvaluablePtr evb;
};

template <class _Evaluable>
bool isType(const Evaluable* evb) {
  if (evb) {
    auto& revb = *evb;  // avoid clang side-effect warning
    return (typeid(revb) == typeid(_Evaluable));
  }
  return false;
}

template <class _Evaluable>
bool isType(const EvaluablePtr& evb) {
  return isType<_Evaluable>(evb.get());
}

struct UseStackEvaluable : public Evaluable {
  UseStackEvaluable(Evaluable* parent, LocalVariablesPtr variables)
      : Evaluable(parent), localVariables{std::move(variables)} {}
  bool useStack() const override { return true; }
  MacroPtr resolveMacro(const StringView& mcname) override {
    if (localMacros) {
      auto itMc = localMacros->find(mcname);
      if (itMc != std::end(*localMacros)) {
        return itMc->second;
      }
    }
    return Evaluable::resolveMacro(mcname);
  }

  bool hasLocalMacros() const { return localMacros && !localMacros->empty(); }

  bool hasLocalVariables() const {
    return localVariables && !localVariables->empty();
  }
  bool hasLocalSymbols() const {
    return hasLocalMacros() || hasLocalVariables();
  }

  virtual String typeID() const = 0;

  LocalVariablesPtr localVariables;
  LocalMacrosMapPtr localMacros;
};

template <class _SpecEvaluable>
struct UseStackEvaluableT : public UseStackEvaluable {
  void accept(EvaluatorIF* e) const override {
    e->eval(static_cast<const _SpecEvaluable&>(*this));
  }
  using __Base = UseStackEvaluable;
  using __Base::__Base;
};

struct StacklessEvaluable : public Evaluable {
  using Evaluable::Evaluable;
  bool useStack() const override { return false; }
};

template <class _SpecEvaluable>
struct StacklessEvaluableT : public StacklessEvaluable {
  using StacklessEvaluable::StacklessEvaluable;
  void accept(EvaluatorIF* e) const override {
    e->eval(static_cast<const _SpecEvaluable&>(*this));
  }
};

struct Constant : public StacklessEvaluableT<Constant> {
  using _Base = StacklessEvaluableT<Constant>;
  template <class T>
  Constant(Evaluable* parent, T&& v)
      : _Base(parent), value(std::forward<T>(v)) {}
  Var value;
};

struct EvaluableDict : public UseStackEvaluableT<EvaluableDict> {
  using __Base = UseStackEvaluableT<EvaluableDict>;
  using ValueType = std::map<String, EvaluablePtr>;
  EvaluableDict(Evaluable* parent, ValueType v = {}, LocalVariablesPtr cp = {})
      : __Base(parent, std::move(cp)), value{std::move(v)} {}

  ValueType value;

  // UseStackEvaluable interface
 public:
  String typeID() const override { return JASSTR("dict"); }
};

struct EvaluableList : public UseStackEvaluableT<EvaluableList> {
  using ValueType = std::vector<EvaluablePtr>;
  using __Base = UseStackEvaluableT<EvaluableList>;
  EvaluableList(Evaluable* parent, ValueType v = {})
      : __Base(parent, LocalVariablesPtr{}), value{std::move(v)} {}
  ValueType value;

  // UseStackEvaluable interface
 public:
  String typeID() const override { return "list"; }
};

template <class _SubType, class _Type>
struct _OperatorBase : public UseStackEvaluableT<_SubType> {
  using Param = Evaluable;
  using ParamPtr = EvaluablePtr;
  using Params = Evaluables;
  using OperatorType = _Type;
  _OperatorBase(Evaluable* parent, OperatorType t, Params p,
                LocalVariablesPtr cp)
      : UseStackEvaluableT<_SubType>(parent, std::move(cp)),
        type{t},
        params{std::move(p)} {}
  void add_param(ParamPtr p) { params.push_back(std::move(p)); }

  OperatorType type;
  Params params;

  // UseStackEvaluable interface
 public:
  String typeID() const override { return strJoin(type); }
};

enum class aot : int32_t {
  plus,
  minus,
  multiplies,
  divides,
  modulus,
  bit_and,
  bit_or,
  bit_not,
  bit_xor,
  negate,
  invalid,
};

inline OStream& operator<<(OStream& os, aot o) {
  switch (o) {
    case aot::plus:
      os << "+";
      break;
    case aot::minus:
      os << "-";
      break;
    case aot::multiplies:
      os << "*";
      break;
    case aot::divides:
      os << "/";
      break;
    case aot::modulus:
      os << "%";
      break;
    case aot::bit_and:
      os << "&";
      break;
    case aot::bit_or:
      os << "|";
      break;
    case aot::bit_not:
      os << "~";
      break;
    case aot::bit_xor:
      os << "xor";
      break;
    case aot::negate:
      os << "-";
      break;
    case aot::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ArithmaticalOperator : public _OperatorBase<ArithmaticalOperator, aot> {
  using _OperatorBase<ArithmaticalOperator, aot>::_OperatorBase;
};

enum class asot : int32_t {
  s_plus,
  s_minus,
  s_multiplies,
  s_divides,
  s_modulus,
  s_bit_and,
  s_bit_or,
  s_bit_xor,
  invalid,
};

inline OStream& operator<<(OStream& os, asot o) {
  switch (o) {
    case asot::s_plus:
      os << "+=";
      break;
    case asot::s_minus:
      os << "-=";
      break;
    case asot::s_multiplies:
      os << "*=";
      break;
    case asot::s_divides:
      os << "/=";
      break;
    case asot::s_modulus:
      os << "%=";
      break;
    case asot::s_bit_and:
      os << "&=";
      break;
    case asot::s_bit_or:
      os << "|=";
      break;
    case asot::s_bit_xor:
      os << "xor=";
      break;
    case asot::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ArthmSelfAssignOperator
    : public _OperatorBase<ArthmSelfAssignOperator, asot> {
  using _OperatorBase<ArthmSelfAssignOperator, asot>::_OperatorBase;
};

enum class lot : char {
  logical_and,
  logical_or,
  logical_not,
  invalid,
};

inline OStream& operator<<(OStream& os, lot o) {
  switch (o) {
    case lot::logical_and:
      os << "&&";
      break;
    case lot::logical_or:
      os << "||";
      break;
    case lot::logical_not:
      os << "!";
      break;
    case lot::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct LogicalOperator : _OperatorBase<LogicalOperator, lot> {
  using _OperatorBase<LogicalOperator, lot>::_OperatorBase;
};

enum class cot : char {
  eq,
  neq,
  lt,
  gt,
  le,
  ge,
  invalid,
};

inline OStream& operator<<(OStream& os, cot o) {
  switch (o) {
    case cot::eq:
      os << "==";
      break;
    case cot::neq:
      os << "!=";
      break;
    case cot::lt:
      os << "<";
      break;
    case cot::gt:
      os << ">";
      break;
    case cot::le:
      os << "<=";
      break;
    case cot::ge:
      os << ">=";
      break;
    case cot::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ComparisonOperator : _OperatorBase<ComparisonOperator, cot> {
  using _OperatorBase<ComparisonOperator, cot>::_OperatorBase;
};

enum class lsaot : char {
  any_of,
  all_of,
  none_of,
  count_if,
  filter_if,
  transform,
  invalid,
};

inline OStream& operator<<(OStream& os, lsaot o) {
  switch (o) {
    case lsaot::any_of:
      os << "any_of";
      break;
    case lsaot::all_of:
      os << "all_of";
      break;
    case lsaot::none_of:
      os << "none_of";
      break;
    case lsaot::count_if:
      os << "count_if";
      break;
    case lsaot::filter_if:
      os << "filter_if";
      break;
    case lsaot::transform:
      os << "transform";
      break;
    case lsaot::invalid:
      os << "invalid";
      break;
  }
  return os;
}

struct ListAlgorithm : public UseStackEvaluableT<ListAlgorithm> {
  ListAlgorithm(Evaluable* parent, lsaot t, EvaluablePtr c,
                EvaluablePtr list = {}, LocalVariablesPtr cp = {})
      : UseStackEvaluableT<ListAlgorithm>(parent, std::move(cp)),
        type(t),
        list(std::move(list)),
        cond(std::move(c)) {}

  lsaot type;
  EvaluablePtr list;
  EvaluablePtr cond;

  // UseStackEvaluable interface
 public:
  String typeID() const override { return strJoin(type); }
};

template <class _SpecificFI>
struct FunctionInvocationBase : public UseStackEvaluableT<_SpecificFI> {
  FunctionInvocationBase(Evaluable* parent, String name, EvaluablePtr param,
                         LocalVariablesPtr cp = {})
      : UseStackEvaluableT<_SpecificFI>(parent, std::move(cp)),
        name(std::move(name)),
        param(std::move(param)) {}

  String name;
  EvaluablePtr param;

  // UseStackEvaluable interface
 public:
  String typeID() const override { return name; }
};

struct ContextFI : public FunctionInvocationBase<ContextFI> {
  using _Base = FunctionInvocationBase<ContextFI>;
  using _Base::_Base;
};

struct ModuleFI : public FunctionInvocationBase<ModuleFI> {
  using _Base = FunctionInvocationBase<ModuleFI>;
  ModuleFI(Evaluable* parent, String name, EvaluablePtr param,
           FunctionModulePtr mdl, LocalVariablesPtr cp = {})
      : _Base(parent, move(name), move(param), std::move(cp)),
        module(std::move(mdl)) {}
  FunctionModulePtr module;
};

struct MacroFI : public FunctionInvocationBase<MacroFI> {
  using _Base = FunctionInvocationBase<MacroFI>;

  MacroFI(Evaluable* parent, String name, MacroPtr macro, EvaluablePtr param,
          LocalVariablesPtr cp = {})
      : _Base(parent, move(name), move(param), std::move(cp)),
        macro(std::move(macro)) {}
  MacroPtr macro;
  // UseStackEvaluable interface
 public:
  String typeID() const { return strJoin('!', name); }
};

struct EvaluatorFI : public FunctionInvocationBase<EvaluatorFI> {
  using _Base = FunctionInvocationBase<EvaluatorFI>;
  using _Base::_Base;
};

struct ObjectPropertyQuery : public StacklessEvaluableT<ObjectPropertyQuery> {
  using _Base = StacklessEvaluableT<ObjectPropertyQuery>;
  ObjectPropertyQuery(Evaluable* parent, EvaluablePtr variable,
                      std::vector<EvaluablePtr> path)
      : _Base(parent),
        object(std::move(variable)),
        propertyPath(std::move(path)) {}

  EvaluablePtr object;
  std::vector<EvaluablePtr> propertyPath;
};

struct Variable : public StacklessEvaluableT<Variable> {
  using _Base = StacklessEvaluableT<Variable>;
  using _Base::_Base;
  Variable(Evaluable* parent, String name)
      : _Base(parent), name(std::move(name)) {}
  String name;
};

struct ContextArgument : public StacklessEvaluableT<ContextArgument> {
  using _Base = StacklessEvaluableT<ContextArgument>;
  using _Base::_Base;
  ContextArgument(Evaluable* parent, int index)
      : _Base(parent), index(std::move(index)) {}
  int index;
};

struct ContextArgumentsInfo : public StacklessEvaluableT<ContextArgumentsInfo> {
  using _Base = StacklessEvaluableT<ContextArgumentsInfo>;
  using _Base::_Base;
  enum class Type { Args, ArgCount };

  ContextArgumentsInfo(Evaluable* parent, Type type)
      : _Base(parent), type(type) {}
  Type type;
};

template <class T>
inline auto makeConst(Evaluable* parent, T&& val) {
  return std::make_shared<Constant>(parent, std::forward<T>(val));
}
inline auto makeOp(Evaluable* parent, aot op, Evaluables params = {},
                   LocalVariablesPtr cp = {}) {
  return std::make_shared<ArithmaticalOperator>(parent, op, std::move(params),
                                                std::move(cp));
}

inline auto makeOp(Evaluable* parent, asot op, Evaluables params = {},
                   LocalVariablesPtr cp = {}) {
  return std::make_shared<ArthmSelfAssignOperator>(
      parent, op, std::move(params), std::move(cp));
}
inline auto makeOp(Evaluable* parent, lot op, Evaluables params = {},
                   LocalVariablesPtr cp = {}) {
  return std::make_shared<LogicalOperator>(parent, op, std::move(params),
                                           std::move(cp));
}
inline auto makeOp(Evaluable* parent, cot op, Evaluables params = {},
                   LocalVariablesPtr cp = {}) {
  return std::make_shared<ComparisonOperator>(parent, op, std::move(params),
                                              std::move(cp));
}
inline auto makeOp(Evaluable* parent, lsaot op, EvaluablePtr cond = {},
                   EvaluablePtr list = {}, LocalVariablesPtr cp = {}) {
  return std::make_shared<ListAlgorithm>(parent, op, std::move(cond),
                                         std::move(list), std::move(cp));
}

template <class _FI>
inline auto makeSimpleFI(Evaluable* parent, String name,
                         EvaluablePtr param = {}, LocalVariablesPtr cp = {}) {
  return std::make_shared<_FI>(parent, std::move(name), std::move(param),
                               std::move(cp));
}

inline auto makeModuleFI(Evaluable* parent, String name,
                         FunctionModulePtr mdl = {}, EvaluablePtr param = {},
                         LocalVariablesPtr cp = {}) {
  return std::make_shared<ModuleFI>(parent, std::move(name), std::move(param),
                                    std::move(mdl), std::move(cp));
}

inline auto makeMacroFI(Evaluable* parent, String name, MacroPtr macro,
                        EvaluablePtr param = {},
                        LocalVariablesPtr stackVars = {}) {
  return std::make_shared<MacroFI>(parent, std::move(name), std::move(macro),
                                   std::move(param), std::move(stackVars));
}

inline auto makeVariable(Evaluable* parent, String name) {
  return std::make_shared<Variable>(parent, std::move(name));
}

inline auto makeCtxtArg(Evaluable* parent, uint8_t pos) {
  return std::make_shared<ContextArgument>(parent, pos);
}

inline auto makeCtxtArgInfo(Evaluable* parent,
                            ContextArgumentsInfo::Type type) {
  return std::make_shared<ContextArgumentsInfo>(parent, type);
}

inline auto makeVariableFieldQuery(Evaluable* parent, EvaluablePtr variable,
                                   std::vector<EvaluablePtr> paths) {
  return std::make_shared<ObjectPropertyQuery>(parent, std::move(variable),
                                               std::move(paths));
}

inline auto makeEvbDict(Evaluable* parent, EvaluableDict::ValueType v = {}) {
  return std::make_shared<EvaluableDict>(parent, std::move(v));
}

inline auto makeEvbList(Evaluable* parent, EvaluableList::ValueType v = {}) {
  return std::make_shared<EvaluableList>(parent, std::move(v));
}

}  // namespace jas
