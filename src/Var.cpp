#include "jas/Var.h"

#include <charconv>
#include <optional>
#include <typeindex>
#include <variant>

#include "jas/Number.h"
#include "jas/TypesName.h"

namespace jas {
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::visit;
using Null = Var::Null;
using Bool = Var::Bool;
using Int = Var::Int;
using Double = Var::Double;
using String = Var::String;
using List = Var::List;
using Dict = Var::Dict;
using Ref = Var::Ref;

#define VAR_POSIB_TYPES Null, Bool, Number, String, List, Dict, Ref
enum class VarTypeIdx : size_t { VAR_POSIB_TYPES };

static constexpr const CharType *indexToType(VarTypeIdx idx) {
  const CharType *ret = JASSTR("");
  switch (idx) {
    case jas::VarTypeIdx::Bool:
      ret = JASSTR("Boolean");
      break;
    case jas::VarTypeIdx::Number:
      ret = JASSTR("Number");
      break;
    case jas::VarTypeIdx::String:
      ret = JASSTR("String");
      break;
    case jas::VarTypeIdx::List:
      ret = JASSTR("List");
      break;
    case jas::VarTypeIdx::Dict:
      ret = JASSTR("Dict");
      break;
    case jas::VarTypeIdx::Ref:
      ret = JASSTR("Ref");
      break;
    case VarTypeIdx::Null:
      ret = JASSTR("Null");
      break;
  }
  return ret;
}

using ValueTypeBase = std::variant<VAR_POSIB_TYPES>;

struct Var::ValueType : public ValueTypeBase {
  using _Base = ValueTypeBase;
  using _Base::_Base;

  decltype(auto) asBase() { return static_cast<_Base &>(*this); }
  decltype(auto) asBase() const { return static_cast<const _Base &>(*this); }

#define __var_get_impl(T)                                                     \
  try {                                                                       \
    return std::get<T>(asBase());                                             \
  } catch (const std::bad_variant_access &) {                                 \
    __jas_throw(TypeError, "Trying cast to type `", nameOfType<T>(),          \
                "` from `",                                                   \
                indexToType(static_cast<VarTypeIdx>(asBase().index())), "`"); \
  }

  template <class T>
  decltype(auto) get() {
    __var_get_impl(T);
  }
  template <class T>
  decltype(auto) get() const {
    __var_get_impl(T);
  }
#undef __var_get_impl
};


template <class _Var, class _Iterator>
static _Var *_find(_Var *j, _Iterator beg, _Iterator end) {
  assert(j);
  for (; beg != end; ++beg) {
    if (j->isDict()) {
      auto &dict = j->asDict();
      if (auto it = dict.find(*beg); it != std::end(dict)) {
        j = &(it->second);
        continue;
      } else {
        j = nullptr;
        break;
      }
    } else if (j->isList()) {
      try {
        auto idx = std::stoul(String{*beg});
        auto &thelist = j->asList();
        if (idx >= thelist.size()) {
          j = nullptr;
          break;
        }
        j = &(thelist[idx]);
      } catch (const std::exception &) {
        j = nullptr;
        break;
      }
    } else {
      j = nullptr;
      break;
    }
  }
  return j;
}

template <class _Var>
static _Var *_find(_Var *j, const Var::Path &path) {
  return jas::_find(j, std::begin(path), std::end(path));
}

#define __Var_type_check(Type)                                               \
  __jas_throw_if(TypeError, !is##Type(), "Trying get ", #Type, " from non ", \
                 #Type, " type")

#define __Var_type_check_after_try_become(Type) \
  if (isNull()) {                               \
    this->value = makeValue<Type>();            \
  } else {                                      \
    __Var_type_check(Type);                     \
  }

template <class T = Null>
auto makeValue(T v = {}) {
  return make_shared<Var::ValueType>(move(v));
}

void throwOutOfRange(bool cond, size_t i) {
  __jas_throw_if(OutOfRange, cond, "Out of bound access: ", i);
}

void throwOutOfRange(bool cond, const StringView &path) {
  __jas_throw_if(OutOfRange, cond, "Path does not exist: ", path);
}

Var::Var() : value(makeValue()) {}
Var::Var(List list) : value(makeValue(move(list))) {}
Var::Var(Dict dict) : value(makeValue(move(dict))) {}
Var::Var(Bool b) : value(makeValue(b)) {}
Var::Var(Int i) : value(makeValue<Number>(i)) {}
Var::Var(Double d) : value(makeValue<Number>(d)) {}
Var::Var(float d) : value(makeValue<Number>(d)) {}
Var::Var(Number d) : value(makeValue<Number>(d)) {}
Var::Var(String in) : value(makeValue(move(in))) {}
Var::Var(Ref in) : value(makeValue(in)) {}
Var::Var(const CharType *in) : value(makeValue<String>(in)) {}
Var::Var(const Json &in) : value(fromJson(in)) {}
Var::Var(Var &&rhs) noexcept {
  value = move(rhs.value);
  rhs.value = makeValue();
}
Var &Var::operator=(Var &&rhs) noexcept {
  if (rhs.value != this->value) {
    value = move(rhs.value);
    rhs.value = makeValue();
  }
  return *this;
}

uint64_t Var::address() const {
  return isRef() ? reinterpret_cast<uint64_t>(asRef().get())
                 : reinterpret_cast<uint64_t>(value.get());
}

long Var::useCount() const { return value.use_count(); }

Json Var::toJson() const {
  if (isDict()) {
    auto out = JsonTrait::object();
    for (auto &[key, val] : asDict()) {
      JsonTrait::add(out, key, val.toJson());
    }
    return JsonTrait::makeJson(out);
  } else if (isList()) {
    auto out = JsonTrait::array();
    for (auto &val : asList()) {
      JsonTrait::add(out, val.toJson());
    }
    return JsonTrait::makeJson(out);
  } else if (isBool()) {
    return JsonTrait::makeJson(getBool());
  } else if (isInt()) {
    return JsonTrait::makeJson(getInt());
  } else if (isString()) {
    return JsonTrait::makeJson(getString());
  } else if (isDouble()) {
    return JsonTrait::makeJson(getDouble());
  } else {
    return {};
  }
}

size_t Var::size() const {
  if (isList()) {
    return asList().size();
  } else if (isDict()) {
    return asDict().size();
  } else {
    __jas_throw(TypeError, "Trying get size of non supported type");
    return 0;
  }
}

bool Var::empty() const {
  if (isList()) {
    return asList().empty();
  } else if (isDict()) {
    return asDict().empty();
  } else if (isString()) {
    return asString().empty();
  } else {
    return isNull();
  }
}

void Var::clear() {
  std::visit(
      [](auto &v) {
        if constexpr (std::is_same_v<std::decay_t<decltype(v)>, Ref>) {
          v->clear();
        } else {
          v = std::decay_t<decltype(v)>{};
        }
      },
      value->asBase());
}

bool Var::contains(const StringView &key) const {
  __Var_type_check(Dict);
  auto &dict = asDict();
  return dict.find(key) != std::end(dict);
}

bool Var::exists(const PathView &path) const {
  return jas::_find(this, path) != nullptr;
}

void Var::add(Var val) {
  __Var_type_check_after_try_become(List);
  asList().push_back(move(val));
}

void Var::add(String key, Var val) {
  __Var_type_check_after_try_become(Dict);
  asDict()[move(key)] = (move(val));
}

const Var &Var::operator[](size_t i) const { return at(i); }

Var &Var::operator[](size_t i) {
  __Var_type_check_after_try_become(List);
  throwOutOfRange(i >= size(), i);
  return asList()[i];
}

Var &Var::operator[](const String &key) {
  __Var_type_check_after_try_become(Dict);
  return asDict()[key];
}

const Var &Var::at(size_t i) const {
  __Var_type_check(List);
  throwOutOfRange(i >= size(), i);
  return asList().at(i);
}

Var Var::getAt(size_t i) const {
  if (isList()) {
    throwOutOfRange(i >= size(), i);
    return at(i);
  }
  return {};
}

const Var &Var::at(const String &key) const {
  __Var_type_check(Dict);
  auto it = asDict().find(key);
  throwOutOfRange(!(it != std::end(asDict())), key);
  return it->second;
}

const Var &Var::atPath(const PathView &path) const {
  auto p = jas::_find(this, path);
  throwOutOfRange(!p, path);
  return *p;
}

Var Var::getAt(const StringView &key) const {
  if (isDict()) {
    if (auto it = asDict().find(key); it != std::end(asDict())) {
      return it->second;
    }
  }
  return {};
}

Var Var::getPath(const PathView &path) const {
  auto pv = jas::_find(this, std::begin(path), std::end(path));
  return pv ? *pv : Var{};
}

size_t Var::erase(size_t i) {
  __Var_type_check(List);
  if (asList().size() <= i) {
    return 0;
  } else {
    asList().erase((asList().begin() + i));
    return 1;
  }
}

size_t Var::erase(const String &key) {
  __Var_type_check(Dict);
  return asDict().erase(key);
}

Var Var::ref(Var other) {
  if (other.isRef()) {
    return make_shared<Var>(*(other.asRef()));
  } else {
    return make_shared<Var>(move(other));
  }
}

Var Var::list(List list) { return Var{move(list)}; }

Var Var::dict(Dict dict) { return Var{move(dict)}; }

size_t Var::typeID() const { return value->index(); }

#define __Var_is_type_impl(Type) \
  std::holds_alternative<Type>(*value) || (isRef() && asRef()->is##Type())

bool Var::isNumber() const { return __Var_is_type_impl(Number); }

bool Var::isString() const { return __Var_is_type_impl(String); }

bool Var::isInt() const { return isNumber() && asNumber().isInt(); }

bool Var::isDouble() const { return isNumber() && asNumber().isDouble(); }

bool Var::isBool() const { return __Var_is_type_impl(Bool); }

bool Var::isList() const { return __Var_is_type_impl(List); }

bool Var::isDict() const { return __Var_is_type_impl(Dict); }

bool Var::isNull() const { return __Var_is_type_impl(Null); }

bool Var::isRef() const { return std::holds_alternative<Ref>(*value); }

#define __Var_as_impl(Type) isRef() ? asRef()->as##Type() : value->get<Type>()

Number &Var::asNumber() { return __Var_as_impl(Number); }

String &Var::asString() { return __Var_as_impl(String); }

bool &Var::asBool() { return __Var_as_impl(Bool); }

List &Var::asList() { return __Var_as_impl(List); }

Dict &Var::asDict() { return __Var_as_impl(Dict); }

Ref &Var::asRef() { return std::get<Ref>(value->asBase()); }

const Number &Var::asNumber() const { return __Var_as_impl(Number); }

const String &Var::asString() const { return __Var_as_impl(String); }

const bool &Var::asBool() const { return __Var_as_impl(Bool); }

const List &Var::asList() const { return __Var_as_impl(List); }

const Dict &Var::asDict() const { return __Var_as_impl(Dict); }

const Ref &Var::asRef() const { return std::get<Ref>(value->asBase()); }

Number Var::getNumber(Number onFailure) const {
  if (isNumber()) {
    return asNumber();
  } else {
    return onFailure;
  }
}

Int Var::getInt(Int onFailure) const { return getNumber(onFailure); }

Double Var::getDouble(Double onFailure) const {
  return static_cast<Double>(getNumber(onFailure));
}

String Var::getString(String onFailure) const {
  if (isString()) {
    return asString();
  }
  return onFailure;
}

Bool Var::getBool(Bool onFailure) const {
  if (isBool()) {
    return asBool();
  }
  return onFailure;
}

const List Var::getList(List onFailure) const {
  if (isList()) {
    return asList();
  }
  return onFailure;
}

const Dict Var::getDict(Dict onFailure) const {
  if (isDict()) {
    return asDict();
  }
  return onFailure;
}

Var &Var::assign(Var e) {
  *value = *e.value;
  return *this;
}

bool Var::detach(long shouldDetachCount) {
  if (isRef()) {
    if (asRef()->useCount() > shouldDetachCount) {
      asRef() = make_shared<Var>(asRef()->clone());
      return true;
    }
  } else if (useCount() > shouldDetachCount) {
    value = make_shared<ValueType>(*value);
    return true;
  }
  return false;
}

void Var::becomeNull() {
  if (useCount() == 1) {
    *value = Null{};
  } else {
    value = makeValue();
  }
}

Var Var::clone() const {
  Var e;
  e.assign(*this);
  return e;
}

String Var::dump() const { return JsonTrait::dump(toJson()); }

Var::ValuePtr Var::fromJson(const Json &json) {
  if (JsonTrait::isObject(json)) {
    Dict dict;
    JsonTrait::iterateObject(json, [&dict](auto &&key, auto &&val) {
      dict.emplace(key, Var{val});
      return true;
    });
    return makeValue(move(dict));
  } else if (JsonTrait::isArray(json)) {
    List lst;
    JsonTrait::iterateArray(json, [&lst](auto &&item) {
      lst.emplace_back(Var{item});
      return true;
    });
    return makeValue(move(lst));
  } else if (JsonTrait::isString(json)) {
    return makeValue(JsonTrait::get<String>(json));
  } else if (JsonTrait::isDouble(json)) {
    return makeValue<Number>(JsonTrait::get<Double>(json));
  } else if (JsonTrait::isInt(json)) {
    return makeValue<Number>(JsonTrait::get<Int>(json));
  } else if (JsonTrait::isBool(json)) {
    return makeValue(JsonTrait::get<Bool>(json));
  } else {
    return makeValue<Null>();
  }
}

bool operator==(const Var &first, const Var &second) {
  if (first.value == second.value) {
    return true;
  } else if (first.isRef()) {
    return *first.asRef() == second;
  } else if (second.isRef()) {
    return first == *second.asRef();
  } else if (first.typeID() == second.typeID()) {
    return std::visit(
        [&second](auto &&v) {
          using PT = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<PT, Number>) {
            return v == second.asNumber();
          } else if constexpr (std::is_same_v<PT, String>) {
            return v == second.asString();
          } else if constexpr (std::is_same_v<PT, List>) {
            return v == second.asList();
          } else if constexpr (std::is_same_v<PT, Dict>) {
            return v == second.asDict();
          } else if constexpr (std::is_same_v<PT, Bool>) {
            return v == second.asBool();
          } else if constexpr (std::is_same_v<PT, Null>) {
            return true;
          } else {
            return false;
          }
        },
        first.value->asBase());
  } else {
    return false;
  }
}

bool operator!=(const Var &first, const Var &second) {
  return !(first == second);
}

bool operator<(const Var &first, const Var &second) {
  if (first.typeID() == second.typeID()) {
    return std::visit(
        [&](auto &&val) {
          using namespace std;
          using PT = decay_t<decltype(val)>;
          if constexpr (is_same_v<bool, PT>) {
            return val < second.asBool();
          } else if constexpr (is_same_v<Number, PT>) {
            return val < second.getNumber();
          } else if constexpr (is_same_v<String, PT>) {
            return val < second.getString();
          } else if constexpr (is_same_v<List, PT>) {
            return val < second.getList();
          } else if constexpr (is_same_v<Dict, PT>) {
            return val < second.asDict();
          } else if constexpr (is_same_v<Var::Ref, PT>) {
            if (val && second.asRef()) {
              return *val < *second.asRef();
            } else {
              return val.get() < second.asRef().get();
            }
          } else {
            return false;
          }
        },
        first.value->asBase());
  } else if (first.isRef()) {
    return first.asRef() ? *first.asRef() < second : true;
  } else if (second.isRef()) {
    return second.asRef() ? first < *second.asRef() : true;
  } else {
    return first.typeID() < second.typeID();
  }
}

bool operator>(const Var &first, const Var &second) { return second < first; }

bool operator>=(const Var &first, const Var &second) {
  return second == first || first > second;
}

bool operator<=(const Var &first, const Var &second) {
  return second == first || first < second;
}

OStream &operator<<(OStream &os, const Var &var) { return os << var.dump(); }
}  // namespace jas
