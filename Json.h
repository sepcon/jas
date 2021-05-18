#pragma once

#include <json11.hpp>

namespace jas {

struct JsonTrait {
  using Json = json11::Json;
  using Object = Json::object;
  using Array = Json::array;
  using String = std::string;
  template <typename T, typename = void>
  struct _Impl;

  static Json parse(const String& s) {
    String err;
    return Json::parse(s, err);
  }

  template <class T>
  static Json makeJson(T&& v) {
    return _Impl<std::decay_t<std::remove_reference_t<T>>>::makeJson(
        std::forward<T>(v));
  }

  template <class T>
  static constexpr bool isJsonConstructible() {
    using Tp = std::remove_const_t<std::remove_reference_t<T>>;
    return std::is_integral_v<Tp> || std::is_floating_point_v<Tp> ||
           std::is_constructible_v<String, T> ||
           std::is_constructible_v<Object, T> ||
           std::is_constructible_v<Array, T>;
  }

  static bool isNull(const Json& j) { return j.is_null(); }
  static bool isDouble(const Json& j) { return j.is_number(); }
  static bool isInt(const Json& j) { return j.is_number(); }
  static bool isBool(const Json& j) { return j.is_bool(); }
  static bool isString(const Json& j) { return j.is_string(); }
  static bool isArray(const Json& j) { return j.is_array(); }
  static bool isObject(const Json& j) { return j.is_object(); }
  template <typename T>
  static bool isType(const Json& j) {
    return _Impl<T>::matchType(j);
  }

  static bool hasKey(const Json& j, const String& key) {
    return !j[key].is_null();
  }
  static bool hasKey(const Object& j, const String& key) {
    auto it = j.find(key);
    return it != j.end();
  }

  static bool equal(const Json& j1, const Json& j2) { return j1 == j2; }

  static bool empty(const Json& j) { return j.is_null(); }

  static size_t size(const Json& j) {
    if (j.is_object()) {
      return j.object_items().size();
    } else if (j.is_array()) {
      return j.array_items().size();
    } else {
      return 0;
    }
  }

  static size_t size(const Array& arr) { return arr.size(); }

  static size_t size(const Object& o) { return o.size(); }

  static Json get(const Json& j, const String& key) { return j[key]; }
  static Json get(const Object& j, const String& key) {
    if (auto it = j.find(key); it != j.end()) {
      return it->second;
    } else {
      return Json{};
    }
  }

  static void add(Object& obj, String key, Json value) {
    obj.emplace(std::move(key), std::move(value));
  }

  static void add(Array& arr, Json value) { arr.push_back(std::move(value)); }

  template <class T>
  static decltype(auto) get(const Json& j) {
    return _Impl<T>::get(j);
  }

  template <class T>
  static decltype(auto) get(const Json& j, const String& key) {
    return _Impl<T>::get(get(j, key));
  }
  template <class _base_type, class _param_type>
  using _allow_if_constructible_t = std::enable_if_t<
      std::is_constructible_v<
          _base_type, std::remove_cv_t<std::remove_reference_t<_param_type>>>,
      void>;

  template <>
  struct _Impl<bool> {
    static bool matchType(const Json& j) { return j.is_bool(); }
    static bool get(const Json& j) { return j.bool_value(); }
    static Json makeJson(bool v) { return Json(v); }
  };

  template <typename T>
  struct _Impl<T, std::enable_if_t<std::is_integral_v<T>, void>> {
    static bool matchType(const Json& j) { return j.is_number(); }
    static T get(const Json& j) { return static_cast<T>(j.int_value()); }
    static Json makeJson(T v) { return static_cast<int>(v); }
  };

  template <typename T>
  struct _Impl<T, std::enable_if_t<std::is_floating_point_v<T>, void>> {
    static bool matchType(const Json& j) { return j.is_number(); }
    static double get(const Json& j) { return j.number_value(); }
    static Json makeJson(T v) { return static_cast<double>(v); }
  };

  template <class _string>
  struct _Impl<_string, _allow_if_constructible_t<String, _string>> {
    static bool matchType(const Json& j) { return j.is_string(); }
    static const String& get(const Json& j) { return j.string_value(); }
    static Json makeJson(_string v) { return Json(std::move(v)); }
  };

  template <>
  struct _Impl<Array> {
    static bool matchType(const Json& j) { return j.is_array(); }
    static const Array& get(const Json& j) { return j.array_items(); }
    static Json makeJson(Array v) { return Json(std::move(v)); }
  };

  template <>
  struct _Impl<Object> {
    static bool matchType(const Json& j) { return j.is_object(); }
    static const Object& get(const Json& j) { return j.object_items(); }
    static Json makeJson(Object v) { return Json(std::move(v)); }
  };

  static String dump(const Json& j) { return j.dump(); }
};

}  // namespace jas

namespace jas {
using Json = JsonTrait::Json;
}  // namespace jas
