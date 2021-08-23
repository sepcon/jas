#pragma once

#include <JsonTrait.h>

#include "Exception.h"

namespace jas {
struct JsonAdapter;
using Json = JsonTrait::Json;
using JsonArray = JsonTrait::Array;
using JsonObject = JsonTrait::Object;
using JsonString = JsonTrait::String;
using JsonPath = JsonTrait::Path;
using JAdapterPtr = std::shared_ptr<JsonAdapter>;

#define __jad_tpl_is_jsonadapter_constructible(T)                             \
  template <class T,                                                          \
            std::enable_if_t<!std::is_same_v<JsonAdapter, std::decay_t<T>> && \
                                 JsonTrait::isJsonConstructible<T>(),         \
                             bool> = true>
struct JsonAdapter {
  __jad_tpl_is_jsonadapter_constructible(T) JsonAdapter(T&& v)
      : value(JsonTrait::makeJson(std::forward<T>(v))) {}
  JsonAdapter(Json v) : value(std::move(v)) {}
  JsonAdapter() = default;
  JsonAdapter(const JsonAdapter&) = default;
  JsonAdapter(JsonAdapter&&) noexcept = default;
  JsonAdapter& operator=(const JsonAdapter&) = default;
  JsonAdapter& operator=(JsonAdapter&&) = default;

  __mc_jas_exception(TypeError);
  template <class T>
  bool isType() const {
    if constexpr (std::is_same_v<
                      Json, std::remove_const_t<std::remove_reference_t<T>>>) {
      return true;
    } else {
      return JsonTrait::isType<T>(value);
    }
  }

  auto type() const { return JsonTrait::type(value); }
  bool isNull() const { return JsonTrait::isNull(value); }
  bool isDouble() const { return JsonTrait::isDouble(value); }
  bool isInt() const { return JsonTrait::isInt(value); }
  bool isBool() const { return JsonTrait::isBool(value); }
  bool isString() const { return JsonTrait::isString(value); }
  bool isArray() const { return JsonTrait::isArray(value); }
  bool isObject() const { return JsonTrait::isObject(value); }
  bool isNumber() const { return isInt() || isDouble(); }

  template <class T>
  T get() const {
    makeSureConvertible<T>();
    if constexpr (std::is_same_v<T, Json>) {
      return value;
    } else {
      return JsonTrait::get<T>(value);
    }
  }

  template <class T>
  T getOr(T&& v) const {
    try {
      return get<T>();
    } catch (const TypeError&) {
      return v;
    }
  }

  template <class T>
  operator T() const {
    return get<T>();
  }

  operator const Json&() const { return value; }
  operator Json&() { return value; }

  template <class T>
  bool convertible() const {
    using PT = std::decay_t<T>;
    if constexpr (!std::is_same_v<bool, PT> &&
                  (std::is_integral_v<PT> || std::is_floating_point_v<PT>)) {
      return isNumber();
    } else {
      return isType<T>();
    }
  }

  template <class T>
  void makeSureConvertible() const {
    throwIf<TypeError>(!convertible<T>(), "Convert from `",
                       JsonTrait::dump(value), "` to ", typeid(T).name());
  }

  template <typename _callable>
  auto visitValue(_callable&& apply, bool throwWhenNull = false) const {
    if (isType<bool>()) {
      return apply(get<bool>());
    } else if (isType<int64_t>()) {
      return apply(get<int64_t>());
    } else if (isType<int32_t>()) {
      return apply(get<int32_t>());
    } else if (isType<double>()) {
      return apply(get<double>());
    } else if (isType<String>()) {
      return apply(get<String>());
    } else if (isType<JsonArray>()) {
      return apply(get<JsonArray>());
    } else if (isType<JsonObject>()) {
      return apply(get<JsonObject>());
    }

    throwIf<TypeError>(throwWhenNull, "vitsit null");
    return apply(value);
  }

  String dump() const { return JsonTrait::dump(value); }

  Json value;
};
#undef __jad_tpl_is_jsonadapter_constructible
}  // namespace jas
