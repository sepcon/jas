#pragma once
#include <boost/json.hpp>
#include <cassert>

namespace jas {

namespace __details {
using namespace boost::json;
using Json = boost::json::value;
template <typename T, typename = void>
struct _TraitImpl;

template <class _base_type, class _param_type>
using _allow_if_constructible_t = std::enable_if_t<
    std::is_constructible_v<
        _base_type, std::remove_cv_t<std::remove_reference_t<_param_type>>>,
    void>;

template <>
struct _TraitImpl<bool> {
  static bool get(const Json& j) { return j.is_bool() ? j.as_bool() : false; }
};

template <typename T>
struct _TraitImpl<T, std::enable_if_t<std::is_integral_v<T>, void>> {
  static T get(const Json& j) {
    return j.is_int64() ? static_cast<T>(j.get_int64())
                        : j.is_uint64() ? static_cast<T>(j.get_uint64()) : T{0};
  }
};

template <typename T>
struct _TraitImpl<T, std::enable_if_t<std::is_floating_point_v<T>, void>> {
  static T get(const Json& j) {
    return static_cast<T>(j.is_double() ? j.get_double() : 0.0);
  }
};

template <class _string>
struct _TraitImpl<_string, _allow_if_constructible_t<std::string, _string>> {
  static std::string get(const Json& j) {
    return j.is_string() ? boost::json::value_to<std::string>(j) : _string{};
  }
};

template <>
struct _TraitImpl<array> {
  static array get(const Json& j) {
    return j.is_array() ? j.as_array() : array{};
  }
};

template <>
struct _TraitImpl<object> {
  static object get(const Json& j) {
    return j.is_object() ? j.get_object() : object{};
  }
};

}  // namespace __details
struct JsonTrait {
  using Json = boost::json::value;
  using Object = boost::json::object;
  using Array = boost::json::array;
  using String = std::string;
  using StringView = boost::json::string_view;

  template <class T>
  static Json makeJson(T&& v) {
    return std::forward<T>(v);
  }
  static auto type(const Json& j) { return j.kind(); }
  static bool isNull(const Json& j) { return j.is_null(); }
  static bool isDouble(const Json& j) { return j.is_double(); }
  static bool isInt(const Json& j) { return j.is_number() && !j.is_double(); }
  static bool isBool(const Json& j) { return j.is_bool(); }
  static bool isString(const Json& j) { return j.is_string(); }
  static bool isArray(const Json& j) { return j.is_array(); }
  static bool isObject(const Json& j) { return j.is_object(); }

  static bool hasKey(const Json& j, const StringView& key) {
    return j.is_object() && j.get_object().contains(key);
  }
  static bool hasKey(const Object& j, const StringView& key) {
    return j.contains(key);
  }

  static bool equal(const Json& j1, const Json& j2) { return j1 == j2; }

  static size_t size(const Json& j) {
    if (j.is_object()) {
      return j.get_object().size();
    } else if (j.is_array()) {
      return j.get_array().size();
    } else {
      return 0;
    }
  }

  static size_t size(const Array& arr) { return arr.size(); }

  static size_t size(const Object& o) { return o.size(); }

  static decltype(auto) get(const Json& j, size_t idx) {
    assert(j.is_array());
    return j.get_array()[idx];
  }

  static Json get(const Json& j, const StringView& path) {
    try {
      return j.as_object().at(path);
    } catch (const std::exception&) {
      return Json{};
    }
  }

  static void add(Object& obj, StringView key, Json value) {
    obj.emplace(std::move(key), std::move(value));
  }

  static void add(Array& arr, Json value) { arr.push_back(std::move(value)); }

  static void add(Json& jarr, Json value) {
    jarr.as_array().push_back(std::move(value));
  }

  static void add(Json& j, StringView key, Json value) {
    j.as_object()[std::move(key)] = std::move(value);
  }

  template <class T>
  static decltype(auto) get(const Json& j) {
    return __details::_TraitImpl<T>::get(j);
  }

  template <class T>
  static decltype(auto) get(const Json& j, const StringView& key) {
    return __details::_TraitImpl<T>::get(JsonTrait::get(j, key));
  }

  template <typename _ElemVisitorCallback>
  static bool iterateArray(const Json& jarr, _ElemVisitorCallback&& visitElem) {
    auto iteratedAll = true;
    for (auto& item : jarr.as_array()) {
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
    for (auto& [k, v] : jobj.as_object()) {
      if (!visitKV(k, v)) {
        iteratedAll = false;
        break;
      }
    }
    return iteratedAll;
  }

  static Object object(Object in = {}) { return std::move(in); }

  static Array array(Array in = {}) { return std::move(in); }

  static Json parse(const StringView& s) {
    boost::json::error_code ec;
    return boost::json::parse(s, ec);
  }

  static Json parse(std::istream& istream) {
    try {
      return parse(String{std::istreambuf_iterator<char>{istream},
                          std::istreambuf_iterator<char>{}});
    } catch (const std::exception&) {
      return {};
    }
  }
  static String dump(const Json& j) { return boost::json::serialize(j); }
};

}  // namespace jas
