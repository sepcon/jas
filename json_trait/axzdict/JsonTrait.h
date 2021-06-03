#pragma once
#include <filesystem>

#include "axz_error_codes.h"
#include "axz_json.h"

namespace jas {

struct JsonTrait {
  using Json = AxzDict;
  using Object = axz_dict_object;
  using Array = axz_dict_array;
  using String = axz_wstring;
  using Path = std::filesystem::path;

  template <typename T, typename = void>
  struct _Impl;

  static Json parse(const String& s) {
    Json j;
    AxzJson::deserialize(s, j);
    return j;
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

  static bool isNull(const Json& j) { return j.isNull(); }
  static bool isDouble(const Json& j) { return j.isNumber(); }
  static bool isInt(const Json& j) { return j.isNumber(); }
  static bool isBool(const Json& j) { return j.isType(AxzDictType::BOOL); }
  static bool isString(const Json& j) { return j.isString(); }
  static bool isArray(const Json& j) { return j.isArray(); }
  static bool isObject(const Json& j) { return j.isObject(); }
  template <typename T>
  static bool isType(const Json& j) {
    return _Impl<T>::matchType(j);
  }

  static bool hasKey(const Json& j, const String& key) {
    return AXZ_SUCCESS(j.contain(key));
  }
  static bool hasKey(const Object& j, const String& key) {
    auto it = j.find(key);
    return it != j.end();
  }

  static bool equal(const Json& j1, const Json& j2) { return j1 == j2; }

  static bool empty(const Json& j) { return j.isNull(); }

  static size_t size(const Json& j) {
    try {
      return j.size();
    } catch (const std::invalid_argument&) {
      return 0;
    }
  }

  static size_t size(const Array& arr) { return arr.size(); }

  static size_t size(const Object& o) { return o.size(); }

  static Json get(const Json& j, const Path& path) {
    return get(&j, std::begin(path), std::end(path));
  }

  static Json get(const Object& j, const Path& path) {
    auto beg = std::begin(path);
    try {
      decltype(auto) first = j.at(beg->wstring());
      return get(&first, ++beg, std::end(path));
    } catch (const std::out_of_range&) {
      return {};
    }
  }

  static Json get(const Json* j, Path::const_iterator beg,
                  Path::const_iterator end) {
    try {
      for (; beg != end; ++beg) {
        if (j->isObject() && AXZ_SUCCESS(j->contain(beg->wstring()))) {
          j = &(j->operator[](beg->wstring()));
          if (j->isNull()) {
            return {};
          }
        } else {
          throw std::runtime_error{""};
        }
      }
      return *j;
    } catch (const std::exception&) {
      return {};
    }
  }

  static void add(Object& obj, String key, Json value) {
    obj.emplace(std::move(key), std::move(value));
  }

  static void add(Array& arr, Json value) { arr.push_back(std::move(value)); }

  template <class T>
  static T get(const Json& j) {
    if (_Impl<T>::matchType(j)) {
      return _Impl<T>::get(j);
    }
    return T{};
  }

  template <class T>
  static decltype(auto) get(const Json& j, const String& key) {
    try {
      return _Impl<T>::get(get(j, key));
    } catch (const std::exception&) {
      return T{};
    }
  }
  template <class _base_type, class _param_type>
  using _allow_if_constructible_t = std::enable_if_t<
      std::is_constructible_v<
          _base_type, std::remove_cv_t<std::remove_reference_t<_param_type>>>,
      void>;

  template <>
  struct _Impl<bool> {
    static bool matchType(const Json& j) { return JsonTrait::isBool(j); }
    static bool get(const Json& j) { return j.boolVal(); }
    static Json makeJson(bool v) { return Json(v); }
  };

  template <typename T>
  struct _Impl<T, std::enable_if_t<std::is_integral_v<T>, void>> {
    static bool matchType(const Json& j) { return j.isNumber(); }
    static T get(const Json& j) { return static_cast<T>(j.numberVal()); }
    static Json makeJson(T v) { return static_cast<int>(v); }
  };

  template <typename T>
  struct _Impl<T, std::enable_if_t<std::is_floating_point_v<T>, void>> {
    static bool matchType(const Json& j) { return j.isNumber(); }
    static double get(const Json& j) { return j.isNumber(); }
    static Json makeJson(T v) { return static_cast<double>(v); }
  };

  template <class _string>
  struct _Impl<_string, _allow_if_constructible_t<String, _string>> {
    static bool matchType(const Json& j) { return j.isString(); }
    static String get(const Json& j) { return j.stringVal(); }
    static Json makeJson(_string v) { return Json(std::move(v)); }
  };

  template <>
  struct _Impl<Array> {
    static bool matchType(const Json& j) { return j.isArray(); }
    static const Array& get(const Json& j) { return j.arrayVal(); }
    static Json makeJson(Array v) { return Json(std::move(v)); }
  };

  template <>
  struct _Impl<Object> {
    static bool matchType(const Json& j) { return j.isObject(); }
    static const Object& get(const Json& j) { return j.objectVal(); }
    static Json makeJson(Object v) { return Json(std::move(v)); }
  };

  static String dump(const Json& j) {
    String out;
    AxzJson::serialize(j, out);
    return out;
  }
};

}  // namespace jas
