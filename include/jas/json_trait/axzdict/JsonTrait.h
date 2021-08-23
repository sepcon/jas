#pragma once
#include <cassert>
#include <filesystem>

#include "axz_error_codes.h"
#include "axz_json.h"

namespace jas {

namespace __details {
using Json = AxzDict;
using String = axz_wstring;
template <class _base_type, class _param_type>
using _allow_if_constructible_t = std::enable_if_t<
    std::is_constructible_v<
        _base_type, std::remove_cv_t<std::remove_reference_t<_param_type>>>,
    void>;

template <typename T, typename = void>
struct _TraitImpl;

template <>
struct _TraitImpl<bool> {
  static auto defaultValue() { return false; }
  static bool matchType(const Json& j) { return j.isType(AxzDictType::BOOL); }
  static bool get(const Json& j) { return j.boolVal(); }
  static Json makeJson(bool v) { return Json(v); }
};

template <typename T>
struct _TraitImpl<T, std::enable_if_t<std::is_integral_v<T>, void>> {
  static T defaultValue() { return 0; }
  static bool matchType(const Json& j) { return j.isNumber(); }
  static T get(const Json& j) { return static_cast<T>(j.intVal()); }
  static Json makeJson(T v) { return static_cast<int>(v); }
};

template <typename T>
struct _TraitImpl<T, std::enable_if_t<std::is_floating_point_v<T>, void>> {
  static T defaultValue() { return 0.0; }
  static bool matchType(const Json& j) { return j.isNumber(); }
  static double get(const Json& j) { return j.numberVal(); }
  static Json makeJson(T v) { return static_cast<double>(v); }
};

template <class _string>
struct _TraitImpl<_string, _allow_if_constructible_t<String, _string>> {
  static _string defaultValue() { return {}; }
  static bool matchType(const Json& j) { return j.isString(); }
  static String get(const Json& j) { return j.stringVal(); }
  static Json makeJson(_string v) { return Json(std::move(v)); }
};

template <>
struct _TraitImpl<axz_dict_array> {
  static const axz_dict_array& defaultValue() {
    static axz_dict_array arr;
    return arr;
  }
  static bool matchType(const Json& j) { return j.isArray(); }
  static const axz_dict_array& get(const Json& j) { return j.arrayVal(); }
  static Json makeJson(axz_dict_array v) { return Json(std::move(v)); }
};

template <>
struct _TraitImpl<axz_dict_object> {
  static const axz_dict_object& defaultValue() {
    static axz_dict_object obj;
    return obj;
  }
  static bool matchType(const Json& j) { return j.isObject(); }
  static const axz_dict_object& get(const Json& j) { return j.objectVal(); }
  static Json makeJson(axz_dict_object v) { return Json(std::move(v)); }
};

}  // namespace __details

struct JsonTrait {
  using Json = AxzDict;
  using Object = axz_dict_object;
  using Array = axz_dict_array;
  using String = axz_wstring;
  using Path = std::filesystem::path;

  template <class T>
  static Json makeJson(T v) {
    using PT = std::decay_t<std::remove_reference_t<T>>;
    return __details::_TraitImpl<PT>::makeJson(std::move(v));
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
  static bool isNull(const Json& j) { return j.isNull(); }
  static bool isDouble(const Json& j) { return j.isNumber(); }
  static bool isInt(const Json& j) { return j.isNumber(); }
  static bool isBool(const Json& j) { return j.isType(AxzDictType::BOOL); }
  static bool isString(const Json& j) { return j.isString(); }
  static bool isArray(const Json& j) { return j.isArray(); }
  static bool isObject(const Json& j) { return j.isObject(); }
  template <typename T>
  static bool isType(const Json& j) {
    return __details::_TraitImpl<T>::matchType(j);
  }

  static bool hasKey(const Json& j, const String& key) {
    return AXZ_SUCCESS(j.contain(key));
  }
  static bool hasKey(const Object& j, const String& key) {
    return j.find(key) != j.end();
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
    obj[std::move(key)] = std::move(value);
  }

  static void add(Array& arr, Json value) { arr.push_back(std::move(value)); }

  static void add(Json& j, Json value) {
    assert(j.isArray());
    j.add(std::move(value));
  }

  static void add(Json& j, const String& key, Json value) {
    assert(j.isObject());
    j.add(key, std::move(value));
  }

  template <class T>
  static decltype(auto) get(const Json& j) {
    if (__details::_TraitImpl<T>::matchType(j)) {
      return __details::_TraitImpl<T>::get(j);
    }
    return __details::_TraitImpl<T>::defaultValue();
  }

  template <class T>
  static decltype(auto) get(const Json& j, const String& key) {
    try {
      return __details::_TraitImpl<T>::get(JsonTrait::get(j, key));
    } catch (const std::exception&) {
      return __details::_TraitImpl<T>::defaultValue();
    }
  }

  static decltype(auto) get(const Json& j, size_t idx) {
    assert(j.isArray());
    return j[idx];
  }

  template <typename _ElemVisitorCallback>
  static bool iterateArray(const Json& jarr, _ElemVisitorCallback&& visitElem) {
    auto iteratedAll = true;
    assert(jarr.isArray());
    for (auto& item : jarr.arrayVal()) {
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
    assert(jobj.isObject());
    for (auto it = std::begin(jobj.objectVal());
         it != std::end(jobj.objectVal()); ++it) {
      if (!visitKV(it->first, it->second)) {
        iteratedAll = false;
        break;
      }
    }
    return iteratedAll;
  }

  static Object object(Object in = {}) { return in; }

  static Array array(Array in = {}) { return in; }

  static Json parse(const String& s) {
    Json j;
    AxzJson::deserialize(s, j);
    return j;
  }

  static Json parse(std::basic_istream<String::value_type>& istream) {
    try {
      using IStreamBufIterator = std::istreambuf_iterator<String::value_type>;
      return parse(String{IStreamBufIterator{istream}, IStreamBufIterator{}});
    } catch (const std::exception&) {
      return {};
    }
  }

  static String dump(const Json& j) {
    String out;
    AxzJson::serialize(j, out);
    return out;
  }
};

}  // namespace jas
