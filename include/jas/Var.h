#pragma once

// TBD: Implement its own path class - don't reply on filesystem path due to
// unicode issue when dealing with both wchar_t and char

#include <map>
#include <vector>

#include "Json.h"
#include "ObjectPath.h"

namespace jas {

class Var {
 public:
  struct ValueType;
  /// Supported Types
  using Ref = std::shared_ptr<Var>;
  using Int = int64_t;
  using Double = double;
  using Bool = bool;
  using String = jas::String;
  using List = std::vector<Var>;
  using Dict = std::map<String, Var, std::less<>>;
  using Path = ObjectPath;
  using PathView = ObjectPathView;
  using ValuePtr = std::shared_ptr<ValueType>;
  struct Null {};

  Var();
  Var(List list);
  Var(Dict dict);
  Var(Bool b);
  Var(Int i);
  Var(Double d);
  Var(float d);
  Var(String in);
  Var(Ref in);
  Var(const CharType* in);
  Var(const Json& in);
  Var(Var&&) noexcept;
  Var& operator=(Var&&) noexcept;
  Var(const Var& rhs) = default;
  Var& operator=(const Var&) = default;

  template <typename _Integer,
            std::enable_if_t<std::is_integral_v<_Integer>, bool> = true>
  Var(_Integer&& i) : Var(static_cast<Int>(i)) {}

  template <typename T, std::enable_if_t<std::is_pointer_v<T>, bool> = true>
  Var(T&& t) = delete;
  Var(std::nullptr_t) = delete;

  uint64_t address() const;
  long useCount() const;

  size_t size() const;
  bool empty() const;
  void clear();

  // List Operation
  void add(Var val);
  Var& operator[](size_t i);
  const Var& operator[](size_t i) const;
  const Var& at(size_t i) const;
  Var getAt(size_t i) const;
  size_t erase(size_t i);

  // Dict operations
  bool contains(const StringView& key) const;
  bool exists(const PathView& path) const;
  size_t erase(const String& key);
  Var& operator[](const String& key);
  const Var& at(const String& key) const;
  const Var& atPath(const PathView& path) const;
  Var getAt(const StringView& key) const;
  Var getPath(const PathView& path) const;
  void add(String key, Var val);

  static Var ref(const Var& rf = {});
  static Var list(List list = {});
  static Var dict(Dict dict = {});

  size_t typeID() const;
  bool isNumber() const;
  bool isString() const;
  bool isInt() const;
  bool isDouble() const;
  bool isBool() const;
  bool isList() const;
  bool isDict() const;
  bool isNull() const;
  bool isRef() const;

  Int& asInt();
  Double& asDouble();
  String& asString();
  bool& asBool();
  List& asList();
  Dict& asDict();

  const Int& asInt() const;
  const Double& asDouble() const;
  const String& asString() const;
  const Bool& asBool() const;
  const List& asList() const;
  const Dict& asDict() const;

  Double getNumber(Double onFailure = 0.0) const;
  Int getInt(Int onFailure = 0) const;
  Double getDouble(Double onFailure = 0) const;
  String getString(String onFailure = {}) const;
  Bool getBool(Bool onFailure = false) const;
  const List getList(List onFailure = {}) const;
  const Dict getDict(Dict onFailure = {}) const;
  template <class T>
  bool isType() const;
  template <class T>
  decltype(auto) getValue(T&& onFailure = {}) const;
  template <typename _callable>
  auto visitValue(_callable&& apply, bool throwWhenNull = false) const;

  Var clone() const;
  Var& assign(Var e);
  bool detachIfUseCountGreaterThan(long shouldDetachCount = 1);
  void becomeNull();
  Json toJson() const;
  String dump() const;

 private:
  static ValuePtr fromJson(const Json& json);
  friend Var operator+(const Var& first, const Var& second);
  friend Var operator+(const Var::List& first, const Var::List& second);
  friend Var operator-(const Var& first, const Var& second);
  friend Var operator*(const Var& first, const Var& second);
  friend Var operator/(const Var& first, const Var& second);
  friend bool operator==(const Var& first, const Var& second);
  friend bool operator!=(const Var& first, const Var& secondr);
  friend bool operator<(const Var& first, const Var& second);
  friend bool operator>(const Var& first, const Var& second);
  friend bool operator>=(const Var& first, const Var& second);
  friend bool operator<=(const Var& first, const Var& second);

  Ref& asRef();
  const Ref& asRef() const;
  ValuePtr value;
};

template <class T>
bool Var::isType() const {
  using PT = std::decay_t<T>;
  if constexpr (std::is_same_v<PT, Bool>) {
    return isBool();
  } else if constexpr (std::is_floating_point_v<PT>) {
    return static_cast<T>(isDouble());
  } else if constexpr (std::is_same_v<PT, String>) {
    return isString();
  } else if constexpr (std::is_integral_v<PT>) {
    return static_cast<T>(isInt());
  } else if constexpr (std::is_same_v<PT, Dict>) {
    return isDict();
  } else if constexpr (std::is_same_v<PT, List>) {
    return isList();
  } else {
    return false;
  }
}
template <class T>
decltype(auto) Var::getValue(T&& onFailure) const {
  using PT = std::decay_t<T>;
  if constexpr (std::is_same_v<PT, Bool>) {
    return getBool(onFailure);
  } else if constexpr (std::is_floating_point_v<PT>) {
    return static_cast<T>(getDouble(onFailure));
  } else if constexpr (std::is_same_v<PT, String>) {
    return getString(onFailure);
  } else if constexpr (std::is_integral_v<PT>) {
    return static_cast<T>(getInt(onFailure));
  } else if constexpr (std::is_same_v<PT, Dict>) {
    return getDict(onFailure);
  } else if constexpr (std::is_same_v<PT, List>) {
    return getList(onFailure);
  } else {
    return onFailure;
  }
}
template <typename _callable>
auto Var::visitValue(_callable&& apply, bool throwWhenNull) const {
  if (isBool()) {
    return apply(getBool());
  } else if (isInt()) {
    return apply(getInt());
  } else if (isDouble()) {
    return apply(getDouble());
  } else if (isString()) {
    return apply(getString());
  } else if (isList()) {
    return apply(getList());
  } else if (isDict()) {
    return apply(getDict());
  } else {
    throwIf<TypeError>(throwWhenNull, "vitsit null");
    return apply(Null{});
  }
}
}  // namespace jas