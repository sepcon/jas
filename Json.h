#pragma once

#include "Exception.h"

#ifdef AXZ
#include "json_trait/axzdict/JsonTrait.h"
#elif USE_NLOHMANN_JSON
#include "json_trait/nlohmann/JsonTrait.h"
#else
#include "json_trait/json11/JsonTrait.h"
#endif

namespace jas {
using Json = JsonTrait::Json;
using JsonArray = JsonTrait::Array;
using JsonObject = JsonTrait::Object;
using JsonString = JsonTrait::String;
using JsonPath = JsonTrait::Path;

struct JsonAdapter {
  template <class T,
            std::enable_if_t<JsonTrait::isJsonConstructible<T>(), bool> = true>
  JsonAdapter(T&& v) : value(JsonTrait::makeJson(std::forward<T>(v))) {}
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

  template <class T>
  bool convertible() const {
    return isType<T>();
  }

  template <class T>
  void makeSureConvertible() const {
    throwIf<TypeError>(!convertible<T>(), "Convert from `",
                       JsonTrait::dump(value), "` to ", typeid(T).name());
  }

  operator const Json&() const { return value; }

  template <typename _callable>
  auto visitValue(_callable&& apply, bool throwWhenNull = false) const {
    if (isType<bool>()) {
      return apply(get<bool>());
    } else if (isType<int>()) {
      return apply(get<int>());
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

  bool isNull() const { return JsonTrait::isNull(value); }
  String dump() const { return JsonTrait::dump(value); }

  Json value;
};

}  // namespace jas
