#pragma once

#include <nlohmann/json.hpp>

namespace jas {

namespace __details {
template <class Json, typename T, typename = void>
struct TraitImpl;
template <class Json>
struct TraitImpl<Json, Json> {
  static bool matchType(const Json& j) { return true; }
};

template <class Json>
struct TraitImpl<Json, bool> {
  static bool matchType(const Json& j) { return j.is_boolean(); }
};

template <class _base_type, class _param_type>
using _allow_if_constructible_t = std::enable_if_t<
    std::is_constructible_v<
        _base_type, std::remove_cv_t<std::remove_reference_t<_param_type>>>,
    void>;

template <class Json, typename T>
struct TraitImpl<Json, T, std::enable_if_t<std::is_integral_v<T>, void>> {
  static bool matchType(const Json& j) { return j.is_number_integer(); }
};

template <class Json, typename T>
struct TraitImpl<Json, T, std::enable_if_t<std::is_floating_point_v<T>, void>> {
  static bool matchType(const Json& j) { return j.is_number_float(); }
};

template <class Json, class _string>
struct TraitImpl<Json, _string,
                 _allow_if_constructible_t<typename Json::string_t, _string>> {
  static bool matchType(const Json& j) { return j.is_string(); }
};

template <class Json>
struct TraitImpl<Json, std::vector<Json>> {
  static bool matchType(const Json& j) { return j.is_array(); }
};

template <class Json>
struct TraitImpl<Json, typename Json::object_t> {
  static bool matchType(const Json& j) { return j.is_object(); }
};

}  // namespace __details
struct JsonTrait {
  using Json = nlohmann::json;
  using String = nlohmann::json::string_t;
  using Object = Json;  //::object_t;
  using Array = Json;

  template <class T>
  static Json makeJson(T&& v) {
    return std::forward<T>(v);
  }

  static auto type(const Json& j) { return j.type(); }
  static bool isNull(const Json& j) { return j.is_null(); }
  static bool isDouble(const Json& j) { return j.is_number_float(); }
  static bool isInt(const Json& j) { return j.is_number_integer(); }
  static bool isBool(const Json& j) { return j.is_boolean(); }
  static bool isString(const Json& j) { return j.is_string(); }
  static bool isArray(const Json& j) { return j.is_array(); }
  static bool isObject(const Json& j) { return j.is_object(); }

  static bool hasKey(const Json& j, const String& key) {
    return j.contains(key);
  }
  static bool equal(const Json& j1, const Json& j2) { return j1 == j2; }

  static size_t size(const Json& j) {
    if (j.is_object() || j.is_array()) {
      return j.size();
    } else {
      return 0;
    }
  }

  static decltype(auto) get(const Json& j, size_t idx) {
    assert(j.is_array());
    return j[idx];
  }

  static Json get(const Object& j, const String& key) {
    if (j.is_object()) {
      auto it = j.find(key);
      if (it != std::end(j)) {
        return *it;
      }
    }
    return {};
  }

  template <class _Json, typename _ElemVisitorCallback>
  static bool iterateArray(_Json&& jarr, _ElemVisitorCallback&& visitElem) {
    auto iteratedAll = true;
    assert(jarr.is_array());
    for (auto it = std::begin(jarr); it != std::end(jarr); ++it) {
      if (!visitElem(*it)) {
        iteratedAll = false;
        break;
      }
    }
    return iteratedAll;
  }

  template <class _Json, typename _KeyValueVisitorCallback>
  static bool iterateObject(_Json&& jobj, _KeyValueVisitorCallback&& visitKV) {
    auto iteratedAll = true;
    assert(jobj.is_object());
    for (auto it = std::begin(jobj); it != std::end(jobj); ++it) {
      if (!visitKV(it.key(), it.value())) {
        iteratedAll = false;
        break;
      }
    }
    return iteratedAll;
  }

  static void add(Json& obj, String key, Json value) {
    assert(obj.is_object());
    obj[std::move(key)] = std::move(value);
  }

  static void add(Json& arr, Json value) {
    assert(arr.is_array());
    arr.push_back(std::move(value));
  }

  template <class T>
  static decltype(auto) get(const Json& j) {
    try {
      return j.get<T>();
    } catch (const Json::exception&) {
      return T{};
    }
  }

  template <class T>
  static decltype(auto) get(const Json& j, const String& key) {
    try {
      return j.at(key).get<T>();
    } catch (const Json::exception&) {
      return T{};
    }
  }

  static Json object(Json::initializer_list_t in = {}) {
    return Json::object(std::move(in));
  }

  static Json array(Json::initializer_list_t in = {}) {
    return Json::array(std::move(in));
  }

  static Json parse(const String& s) {
    try {
      return Json::parse(s);
    } catch (const Json::exception&) {
      return {};
    }
  }
  static Json parse(std::istream& s) {
    try {
      return Json::parse(s);
    } catch (const Json::exception&) {
      return {};
    }
  }

  static String dump(const Json& j) { return j.dump(); }
};

}  // namespace jas
