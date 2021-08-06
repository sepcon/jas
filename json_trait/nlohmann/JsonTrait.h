#pragma once

#include <filesystem>

#include "json.hpp"

namespace jas {

namespace __details {
template <class Json, typename T, typename = void>
struct TraitImpl;
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
struct TraitImpl<Json, _string, _allow_if_constructible_t<String, _string>> {
  static bool matchType(const Json& j) { return j.is_string(); }
};

template <class Json>
struct TraitImpl<Json, std::vector<Json>> {
  static bool matchType(const Json& j) { return j.is_array(); }
};

template <class Json>
struct TraitImpl<Json, std::map<String, Json>> {
  static bool matchType(const Json& j) { return j.is_object(); }
};

}  // namespace __details
struct JsonTrait {
  using String = std::string;
  using Json = nlohmann::json;
  using Object = std::map<String, Json>;
  // can be Json only but some situation we can not differentiate array vs json
  // :D
  using Array = std::vector<Json>;
  using Path = std::filesystem::path;

  static Json parse(const String& s) {
    try {
      return Json::parse(s);
    } catch (const Json::exception&) {
      return {};
    }
  }

  template <class T>
  static Json makeJson(T&& v) {
    Json j = v;
    return j;
  }

  template <class T>
  static constexpr bool isJsonConstructible() {
    using Tp = std::remove_const_t<std::remove_reference_t<T>>;
    return std::is_integral_v<Tp> || std::is_floating_point_v<Tp> ||
           std::is_constructible_v<String, T> ||
           std::is_constructible_v<Object, T> ||
           std::is_constructible_v<Array, T>;
  }

  static auto type(const Json& j) { return j.type(); }
  static bool isNull(const Json& j) { return j.is_null(); }
  static bool isDouble(const Json& j) { return j.is_number_float(); }
  static bool isInt(const Json& j) { return j.is_number_integer(); }
  static bool isBool(const Json& j) { return j.is_boolean(); }
  static bool isString(const Json& j) { return j.is_string(); }
  static bool isArray(const Json& j) { return j.is_array(); }
  static bool isObject(const Json& j) { return j.is_object(); }
  template <typename T>
  static bool isType(const Json& j) {
    return __details::TraitImpl<Json, T>::matchType(j);
  }

  static bool hasKey(const Json& j, const String& key) {
    return j.contains(key);
  }
  static bool hasKey(const Object& j, const String& key) {
    auto it = j.find(key);
    return it != j.end();
  }

  static bool equal(const Json& j1, const Json& j2) { return j1 == j2; }

  static bool empty(const Json& j) { return j.is_null(); }

  static size_t size(const Json& j) {
    if (j.is_object() || j.is_array()) {
      return j.size();
    } else {
      return 0;
    }
  }
  static size_t size(const Array& j) { return j.size(); }

  static size_t size(const Object& o) { return o.size(); }

  //  static Json get(const Json& j, const String& key) { return j[key]; }
  //  static Json get(const Object& j, const String& key) {
  //    if (auto it = j.find(key); it != j.end()) {
  //      return it->second;
  //    } else {
  //      return Json{};
  //    }
  //  }

  static Json get(const Json& j, const Path& path) {
    return get(&j, std::begin(path), std::end(path));
  }

  static Json get(const Object& j, const Path& path) {
    auto beg = std::begin(path);
    try {
      decltype(auto) first = j.at(beg->u8string());
      return get(&first, ++beg, std::end(path));
    } catch (const std::out_of_range&) {
      return {};
    }
  }

  static Json get(const Json* j, Path::const_iterator beg,
                  Path::const_iterator end) {
    for (; beg != end; ++beg) {
      try {
        j = &(j->at(beg->u8string()));
        if (j->is_null()) {
          return {};
        }
      } catch (const Json::exception&) {
        return {};
      }
    }
    return *j;
  }

  static void add(Object& obj, String key, Json value) {
    obj.emplace(std::move(key), std::move(value));
  }

  static void add(Array& arr, Json value) { arr.push_back(std::move(value)); }

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
  static String dump(const Json& j) { return j.dump(); }
};

}  // namespace jas
