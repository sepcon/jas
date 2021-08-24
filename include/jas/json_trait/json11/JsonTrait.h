#pragma once
#include <cassert>
#include <json11.hpp>

namespace jas {

namespace __details {
using namespace json11;
template <typename T, typename = void>
struct _TraitImpl;

template <class _base_type, class _param_type>
using _allow_if_constructible_t = std::enable_if_t<
    std::is_constructible_v<
        _base_type, std::remove_cv_t<std::remove_reference_t<_param_type>>>,
    void>;

template <>
struct _TraitImpl<bool> {
  static bool matchType(const Json& j) { return j.is_bool(); }
  static bool get(const Json& j) { return j.bool_value(); }
  static Json makeJson(bool v) { return Json(v); }
};

template <typename T>
struct _TraitImpl<T, std::enable_if_t<std::is_integral_v<T>, void>> {
  static bool matchType(const Json& j) { return j.is_number(); }
  static T get(const Json& j) { return static_cast<T>(j.int_value()); }
  static Json makeJson(T v) { return static_cast<int>(v); }
};

template <typename T>
struct _TraitImpl<T, std::enable_if_t<std::is_floating_point_v<T>, void>> {
  static bool matchType(const Json& j) { return j.is_number(); }
  static double get(const Json& j) { return j.number_value(); }
  static Json makeJson(T v) { return static_cast<double>(v); }
};

template <class _string>
struct _TraitImpl<_string, _allow_if_constructible_t<std::string, _string>> {
  static bool matchType(const Json& j) { return j.is_string(); }
  static const std::string& get(const Json& j) { return j.string_value(); }
  static Json makeJson(_string v) { return std::string{v}; }
};

template <>
struct _TraitImpl<Json::array> {
  static bool matchType(const Json& j) { return j.is_array(); }
  static const Json::array& get(const Json& j) { return j.array_items(); }
  static Json makeJson(Json::array v) { return Json(std::move(v)); }
};

template <>
struct _TraitImpl<Json::object> {
  static bool matchType(const Json& j) { return j.is_object(); }
  static const Json::object& get(const Json& j) { return j.object_items(); }
  static Json makeJson(Json::object v) { return Json(std::move(v)); }
};

}  // namespace __details
struct JsonTrait {
  using Json = json11::Json;
  using Object = Json::object;
  using Array = Json::array;
  using String = std::string;

  template <class T>
  static Json makeJson(T&& v) {
    return __details::_TraitImpl<
        std::decay_t<std::remove_reference_t<T>>>::makeJson(std::forward<T>(v));
  }
  static auto type(const Json& j) { return j.type(); }
  static bool isNull(const Json& j) { return j.is_null(); }
  static bool isDouble(const Json& j) { return j.is_number(); }
  static bool isInt(const Json& j) { return j.is_number(); }
  static bool isBool(const Json& j) { return j.is_bool(); }
  static bool isString(const Json& j) { return j.is_string(); }
  static bool isArray(const Json& j) { return j.is_array(); }
  static bool isObject(const Json& j) { return j.is_object(); }

  static bool hasKey(const Json& j, const String& key) {
    return !j[key].is_null();
  }
  static bool hasKey(const Object& j, const String& key) {
    auto it = j.find(key);
    return it != j.end();
  }

  static bool equal(const Json& j1, const Json& j2) { return j1 == j2; }

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

  static decltype(auto) get(const Json& j, size_t idx) {
    assert(j.is_array());
    return j[idx];
  }

  static Json get(const Json& j, const String& path) { return j[path]; }

  static void add(Object& obj, String key, Json value) {
    obj[std::move(key)] = std::move(value);
  }

  static void add(Array& arr, Json value) { arr.push_back(std::move(value)); }

  static void add(Json& jarr, Json value) {
    assert(jarr.is_array());
    auto& arr = const_cast<Array&>(jarr.array_items());
    arr.push_back(std::move(value));
  }

  static void add(Json& j, String key, Json value) {
    assert(j.is_object());
    auto& arr = const_cast<Object&>(j.object_items());
    arr[std::move(key)] = std::move(value);
  }

  template <class T>
  static decltype(auto) get(const Json& j) {
    return __details::_TraitImpl<T>::get(j);
  }

  template <class T>
  static decltype(auto) get(const Json& j, const String& key) {
    return __details::_TraitImpl<T>::get(JsonTrait::get(j, key));
  }

  template <typename _ElemVisitorCallback>
  static bool iterateArray(const Json& jarr, _ElemVisitorCallback&& visitElem) {
    auto iteratedAll = true;
    assert(jarr.is_array());
    for (auto& item : jarr.array_items()) {
      if (!visitElem(item)) {
        iteratedAll = false;
        break;
      }
    }
    return iteratedAll;
  }

  template <typename _KeyValueVisitorCallback>
  static bool iterateObject(const Json& jobj,
                            _KeyValueVisitorCallback&& visitKV) {
    auto iteratedAll = true;
    assert(jobj.is_object());
    for (auto it = std::begin(jobj.object_items());
         it != std::end(jobj.object_items()); ++it) {
      if (!visitKV(it->first, it->second)) {
        iteratedAll = false;
        break;
      }
    }
    return iteratedAll;
  }

  static Object object(Object in = {}) { return Json::object(std::move(in)); }

  static Array array(Array in = {}) { return Json::array(std::move(in)); }

  static Json parse(const String& s) {
    String err;
    return Json::parse(s, err);
  }

  static Json parse(std::istream& istream) {
    try {
      return parse(String{std::istreambuf_iterator<char>{istream},
                          std::istreambuf_iterator<char>{}});
    } catch (const std::exception&) {
      return {};
    }
  }
  static String dump(const Json& j) { return j.dump(); }
};

}  // namespace jas
