#include <cctype>
#include <fstream>
#include <iostream>

#include "D:\SourceCode\GitHub\json11\json11.hpp"
#include "ctime"
#include "jas_result_evaluator.h"
#include "jas_syntax_generator.h"

namespace jas {
inline time_t current_time() { return std::time(nullptr); }

template <class _transformer>
string_type transform(string_type s, _transformer&& tr) {
  std::transform(std::begin(s), std::end(s), std::begin(s), tr);
  return s;
}

inline string_type tolower(string_type input) {
  return transform(input, std::tolower);
}

inline string_type toupper(string_type input) {
  return transform(input, std::toupper);
}

}  // namespace jas

using namespace std;

using namespace jas;
using namespace sytax_dump;

namespace jas {

struct my_trait {
  using json_type = json11::Json;
  using object_type = json_type::object;
  using array_type = json_type::array;
  using string_type = std::string;
  template <typename T, typename = void>
  struct Impl;

  static bool is_null(const json_type& j) { return j.is_null(); }
  static bool is_double(const json_type& j) { return j.is_number(); }
  static bool is_int(const json_type& j) { return j.is_number(); }
  static bool is_bool(const json_type& j) { return j.is_bool(); }
  static bool is_string(const json_type& j) { return j.is_string(); }
  static bool is_array(const json_type& j) { return j.is_array(); }
  static bool is_object(const json_type& j) { return j.is_object(); }

  static bool equal(const json_type& j1, const json_type& j2) {
    return j1 == j2;
  }

  static bool empty(const json_type& j) { return j.is_null(); }

  static size_t size(const json_type& j) {
    if (j.is_object()) {
      return j.object_items().size();
    } else if (j.is_array()) {
      return j.array_items().size();
    } else {
      return 0;
    }
  }

  static size_t size(const array_type& arr) { return arr.size(); }

  static size_t size(const object_type& o) { return o.size(); }

  static json_type get(const json_type& j, const string_type& key) {
    return j[key];
  }

  template <class T>
  static T get(const json_type& j) {
    return Impl<T>::get(j);
  }

  template <class T>
  static T get(const json_type& j, const string_type& key) {
    return Impl<T>::get(get(j, key));
  }
  template <class RawType, class Type>
  using IsTypeOf = std::enable_if_t<
      std::is_same_v<RawType, std::remove_cv_t<std::remove_reference_t<Type>>>,
      void>;

  template <>
  struct Impl<bool> {
    static bool get(const json_type& j) { return j.bool_value(); }
  };

  template <typename T>
  struct Impl<T, std::enable_if_t<std::is_integral_v<T>, void>> {
    static int get(const json_type& j) { return j.int_value(); }
  };

  template <>
  struct Impl<double> {
    static double get(const json_type& j) { return j.number_value(); }
  };
  template <class String>
  struct Impl<String, IsTypeOf<std::string, String>> {
    static String get(const json_type& j) { return j.string_value(); }
  };

  template <class Array>
  struct Impl<Array, IsTypeOf<json_type::array, Array>> {
    static json_type::array get(const json_type& j) { return j.array_items(); }
  };

  template <class JsonObject>
  struct Impl<JsonObject, IsTypeOf<json_type::object, JsonObject>> {
    static JsonObject get(const json_type& j) { return j.object_items(); }
  };

  static string_type dump(const json_type& j) { return j.dump(); }
};

template <class _json_trait>
class json_context : public evaluation_context_base {
  using jtrait_type = _json_trait;
  using json_type = typename jtrait_type::json_type;
  using jobject_type = typename jtrait_type::object_type;
  using jarray_type = typename jtrait_type::array_type;
  using jstring_type = typename jtrait_type::string_type;

 public:
  using snapshot_idx = context_value_collector::snapshot_idx_val;
  json_context(json_type new_snapshot, json_type old_snapshot = {})
      : snapshots_{std::move(old_snapshot), std::move(new_snapshot)} {}

  string_type dump() const override {
    return "";  // jtrait_type::dump(snapshots_[snapshot_idx::new_snapshot_idx]);
  }
  virtual evaluated_value invoke_function(const string_type& /*function_name*/,
                                          evaluated_value /*param*/) override {
    return evaluated_value{current_time()};
  }

  virtual evaluated_value extract_value(const string_type& value_path,
                                        size_t snapshot_idx) override {
    if (snapshot_idx < 2) {
      auto jval = jtrait_type::get(snapshots_[snapshot_idx], value_path);
      return convert(jval);
    }
    return {};
  }

  virtual evaluation_contexts extract_sub_contexts(
      const string_type& list_path, const string_type& item_id) override {
    auto jnew_val =
        jtrait_type::get(snapshots_[snapshot_idx::new_snapshot_idx], list_path);
    auto jold_val =
        jtrait_type::get(snapshots_[snapshot_idx::old_snapshot_idx], list_path);
    evaluation_contexts ctxts;
    auto new_list =
        list_path.empty()
            ? jtrait_type::template get<jarray_type>(
                  snapshots_[snapshot_idx::new_snapshot_idx])
            : jtrait_type::template get<jarray_type>(
                  snapshots_[snapshot_idx::new_snapshot_idx], list_path);
    auto old_list =
        list_path.empty()
            ? jtrait_type::template get<jarray_type>(
                  snapshots_[snapshot_idx::old_snapshot_idx])
            : jtrait_type::template get<jarray_type>(
                  snapshots_[snapshot_idx::old_snapshot_idx], list_path);
    auto find_item = [&item_id](const jarray_type& list,
                                const json_type& item_id_val) {
      for (auto& item : list) {
        if (jtrait_type::equal(jtrait_type::get(item, item_id), item_id_val)) {
          return item;
        }
      }
      return json_type{};
    };

    if (!item_id.empty()) {
      for (auto& new_item : new_list) {
        auto old_item =
            find_item(old_list, jtrait_type::get(new_item, item_id));
        ctxts.emplace_back(new json_context{new_item, old_item});
      }
    } else {
      auto old_list_size = jtrait_type::size(old_list);
      auto new_list_size = jtrait_type::size(new_list);
      jarray_type* longger_list = nullptr;
      jarray_type* shorter_list = nullptr;
      std::function<void(json_type, json_type)> add_ctxt;
      if (old_list_size < new_list_size) {
        shorter_list = &old_list;
        longger_list = &new_list;
        add_ctxt = [&ctxts](auto sitem, auto litem) {
          ctxts.emplace_back(
              new json_context{std::move(litem), std::move(sitem)});
        };

      } else {
        shorter_list = &new_list;
        longger_list = &old_list;
        add_ctxt = [&ctxts](auto sitem, auto litem) {
          ctxts.emplace_back(
              new json_context{std::move(sitem), std::move(litem)});
        };
      }

      auto siter = std::begin(*shorter_list);
      auto liter = std::begin(*longger_list);
      while (siter != std::end(*shorter_list)) {
        add_ctxt(*siter, *liter);
        ++siter;
        ++liter;
      }
      while (liter != std::end(*longger_list)) {
        add_ctxt(json_type{}, *liter);
        ++liter;
      }
    }

    return ctxts;
  }

  static evaluated_value convert(const json_type& jval) {
    if (jtrait_type::is_int(jval)) {
      return evaluated_value{jtrait_type::template get<int64_t>(jval)};
    } else if (jtrait_type::is_double(jval)) {
      return evaluated_value{jtrait_type::template get<double>(jval)};
    } else if (jtrait_type::is_string(jval)) {
      return evaluated_value{jtrait_type::template get<string_type>(jval)};
    } else if (jtrait_type::is_bool(jval)) {
      return evaluated_value{jtrait_type::template get<bool>(jval)};
    } else {
      return evaluated_value{};
    }
  }

 protected:
  json_type snapshots_[2];
};

}  // namespace jas

#include "jas_parser.h"

int main() {
  using namespace json11;
  string serr;
  auto jrules = Json::parse(
      R"(
{"@none_of":{"@list_path":"","@item_id":"ID","@cond":{"@ge":[{"@ge":[{"@func":""},{"@key":"data_file_time"},{"@key":"data_file_time"}, 1, 2, 3]},259200]}}}
)",
      serr);

  auto joldDoc = Json::parse(R"(
{"Antivirus":[{"Data File DateTime":"","Data File Engine Version":"","Data File Version":"","ID":11115,"IsDataFileDateTimeSupported":1,"IsLastFullScanTimeSupported":0,"IsRunningSupported":1,"Last Full Scan Time":"","PRODUCT_NAME":"Norton Security Scan","Running":0,"Threats":[],"VENDOR":"Symantec Corporation","VERSION":""},{"Data File DateTime":"2021/4/1 14:49:8","Data File Engine Version":"1.1.17900.7","Data File Version":"1.333.1767.0","ID":477,"IsDataFileDateTimeSupported":1,"IsLastFullScanTimeSupported":1,"IsRunningSupported":1,"Last Full Scan Time":"","PRODUCT_NAME":"Windows Defender","Running":1,"Threats":[],"VENDOR":"Microsoft Corporation","VERSION":"4.18.2102.4"}]}
)",
                             serr);

  auto jnewDoc = Json::parse(R"(
{"Antivirus":[{"data_file_time":1620779950,"Data File DateTime":"","Data File Engine Version":"","Data File Version":"","ID":1115,"IsDataFileDateTimeSupported":1,"IsLastFullScanTimeSupported":0,"IsRunningSupported":0,"Last Full Scan Time":"","PRODUCT_NAME":"Norton Security Scan","Running":0,"Threats":[],"VENDOR":"Symantec Corporation","VERSION":""},{"data_file_time":1621064816,"Data File DateTime":"","Data File Engine Version":"","Data File Version":"","ID":11125,"IsDataFileDateTimeSupported":1,"IsLastFullScanTimeSupported":0,"IsRunningSupported":0,"Last Full Scan Time":"","PRODUCT_NAME":"Norton Security Scan","Running":0,"Threats":[],"VENDOR":"Symantec Corporation","VERSION":""}]}
)",
                             serr);
  std::ofstream ofs{"error.log"};

  try {
    auto evaluable = parser<my_trait>::parse(jrules);
    cout << "Genrated syntax: " << endl;
    ostringstream_type oss;
    ofs << *evaluable;
    oss << *evaluable;
    cout << oss.str();
    auto context =
        make_shared<json_context<my_trait>>(jnewDoc["Antivirus"] /*, joldDoc*/);
    syntax_evaluator evaluator(context);
    evaluable->accept(&evaluator);
    cout << "-------------------\n";
    cout << "evaluated result =" << evaluator.evaluated_result() << endl;
  } catch (const exception_base& e) {
    cout << e.what() << endl;
  }
  return 0;

  auto o = make_op(
      list_operation_type::any_of,
      make_op(arithmetic_operator_type::modulus,
              {make_op(arithmetic_operator_type::plus,
                       vector<shared_ptr<evaluable_base>>{
                           make_op(arithmetic_operator_type::divides,
                                   {make_ctxt_value_collector("date_of_birth"),
                                    2_const}),
                           20_const,
                           make_op(arithmetic_operator_type::negate,
                                   {10_const, 23_const})}),
               2_const}),
      "hello world");

  auto c = "21/08/1991"_const;

  Json data = Json::object{
      {"hello world", Json::array{Json::object{{"date_of_birth", 10}}}}};

  try {
    syntax_evaluator e(make_shared<json_context<my_trait>>(/*Json{},*/ data));
    ostringstream_type oss;
    oss << o;
    cout << oss.str();
    o->accept(&e);
    cout << e.evaluated_result() << endl;
  } catch (const exception_base& e) {
    cout << "got exception: " << typeid(e).name() << "\n" << e.what() << endl;
  }
  return 0;
}
