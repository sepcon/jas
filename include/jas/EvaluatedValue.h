#pragma once

#include <map>
#include <variant>
#include <vector>

#include "Json.h"

namespace jas {

class EvaluatedValue {
  using CSingle = JsonAdapter;
  using CList = std::vector<EvaluatedValue>;
  using CMap = std::map<String, EvaluatedValue, std::less<>>;

  using Single = std::shared_ptr<CSingle>;
  using List = std::shared_ptr<CList>;
  using Map = std::shared_ptr<CMap>;

// Future defined types must be added here
#define __ev_PosibleTypes Single, List, Map

  enum class ID : size_t { __ev_PosibleTypes };

  using ValueType = std::variant<__ev_PosibleTypes>;
  static constexpr size_t idx(ID idx) { return static_cast<size_t>(idx); }

  template <ID i>
  constexpr decltype(auto) get() {
    return std::get<idx(i)>(value);
  }
  template <ID i>
  constexpr decltype(auto) get() const {
    return std::get<idx(i)>(value);
  }

 public:
  EvaluatedValue() = default;
  EvaluatedValue(List arr);
  EvaluatedValue(CList arr);
  EvaluatedValue(Map obj);
  EvaluatedValue(CMap obj);
  EvaluatedValue(Single Single);
  EvaluatedValue(CSingle Single);

  //  template <class T,
  //            std::enable_if_t<std::is_constructible_v<Single, T>, bool> =
  //            true>
  //  EvaluatedValue(T&& v) :
  //  value(std::make_shared<Single>(std::forward<T>(v))) {}
  EvaluatedValue(EvaluatedValue&&) = default;
  EvaluatedValue& operator=(EvaluatedValue&&) = default;
  EvaluatedValue(const EvaluatedValue&) = default;
  EvaluatedValue& operator=(const EvaluatedValue&) = default;

#define __ev_DefineTypeExplorationMethods(Type) \
  constexpr decltype(auto) get##Type() {        \
    return std::get<idx(ID::Type)>(value);      \
  }                                             \
  constexpr decltype(auto) get##Type() const {  \
    return std::get<idx(ID::Type)>(value);      \
  }                                             \
  constexpr bool is##Type() const { return value.index() == idx(ID::Type); }

  __ev_DefineTypeExplorationMethods(Map);
  __ev_DefineTypeExplorationMethods(List);
  __ev_DefineTypeExplorationMethods(Single);

  Json toJson() const;
  size_t size() const;
  void add(EvaluatedValue val);
  void add(String key, EvaluatedValue val);
  EvaluatedValue& at(size_t i);
  EvaluatedValue& at(const String& key);
  EvaluatedValue& at(size_t i) const;
  EvaluatedValue& at(const String& key) const;
  size_t erase(size_t i);
  size_t erase(const String& key);
  static EvaluatedValue list(CList arr = {});
  static EvaluatedValue map(CMap obj = {});
  static EvaluatedValue single(CSingle Single = {});

 private:
  ValueType value;
#undef __ev_PosibleTypes
#undef __ev_DefineTypeExplorationMethods
};

}  // namespace jas
