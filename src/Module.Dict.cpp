#include "VarManipModuleShared.h"

namespace jas {
namespace mdl {
namespace dict {

#define __dict_func(name, params) __module_func(dict, name, params)

#define __dict_verify_no_args(params)               \
  __jas_func_throw_invalidargs_if(!params.isDict(), \
                                  "first argument is not a dict", params)

#define __dict_verify_args__(params, additional_cond)                      \
  __jas_func_throw_invalidargs_if(!params.isList() || additional_cond,     \
                                  "un-match number of arguments", params); \
  __dict_verify_no_args(params[0])

#define __dict_verify_args(params) \
  __dict_verify_args__(params, params.size() < 2)

#define __dict_verify_args_fit_count(params, count) \
  __dict_verify_args__(params, !(params.size() == count))

__dict_func(update, params) {
  __dict_verify_args(params);
  // update:["$dict", {"key": value, "key2": "value2"}]
  // update:["$dict", "key", value}]

  if (params.size() == 2) {
    __jas_func_throw_invalidargs_if(
        !params[1].isDict() || params[1].size() == 0,
        "Expect an object of {key:value, ...}", params[1]);
    auto& dict = variable_detach(params[0]);
    auto& additionalDict = params[1].asDict();
    for (auto& [key, val] : additionalDict) {
      dict.add(key, val);
    }
    return dict;
  } else if (params.size() == 3) {
    __jas_func_throw_invalidargs_if(!params[1].isString(),
                                    "Expect a string of key", params[1]);
    auto& dict = variable_detach(params[0]);
    auto& key = params[1].asString();
    dict.add(key, params[2]);
    return dict;
  } else {
    __jas_func_throw_invalidargs("Redundant argument", params[3]);
    return {};
  }
}

__dict_func(erase, params) {
  // erase:["$dict", "key"]
  __dict_verify_args_fit_count(params, 2);
  __jas_func_throw_invalidargs_if(!params[1].isString(),
                                  "expect a string of key", params[1]);
  auto& dict = variable_detach(params[0]);
  dict.erase(params[1].asString());
  return dict;
}

__dict_func(get, params) {
  // get:["$dict", "key"]
  __dict_verify_args_fit_count(params, 2);
  __jas_func_throw_invalidargs_if(!params[1].isString(),
                                  "expect a string of key", params[1]);
  return params[0].getAt(params[1].asString());
}

__dict_func(get_path, params) {
  // get_path:["$dict", "path/to/value"]
  __dict_verify_args_fit_count(params, 2);
  __jas_func_throw_invalidargs_if(!params[1].isString(),
                                  "expect a string of key", params[1]);
  return params[0].getPath(params[1].asString());
}

__dict_func(contains, params) {
  // contains:["$dict", "key"]
  __dict_verify_args_fit_count(params, 2);
  __jas_func_throw_invalidargs_if(!params[1].isString(),
                                  "expect a string of key", params[1]);
  return params[0].contains(params[1].asString());
}

__dict_func(exists, params) {
  // exists:["$dict", "path/to/value"]
  __dict_verify_args_fit_count(params, 2);
  __jas_func_throw_invalidargs_if(!params[1].isString(),
                                  "expect a string of key", params[1]);
  return params[0].exists(params[1].asString());
}

__dict_func(clear, thedict) {
  // contains:"$dict"
  __dict_verify_no_args(thedict);
  variable_detach(thedict).asDict().clear();
  return thedict;
}

__dict_func(keys, thedict) {
  // keys:"$dict"
  __dict_verify_no_args(thedict);
  auto keys = Var::list();
  for (auto& [key, val] : thedict.asDict()) {
    keys.add(key);
  }
  return keys;
}

__dict_func(values, thedict) {
  // values:"$dict"
  __dict_verify_no_args(thedict);
  auto values = Var::list();
  for (auto& [key, val] : thedict.asDict()) {
    values.add(val);
  }
  return values;
}

__dict_func(is_empty, thedict) {
  // is_empty:"$dict"
  __dict_verify_no_args(thedict);
  return thedict.empty();
}

__dict_func(size, thedict) {
  // size:"$dict"
  __dict_verify_no_args(thedict);
  return thedict.size();
}

__module_class_begin(dict){
    __module_register_func(dict, update),
    __module_register_func(dict, erase),
    __module_register_func(dict, clear),
    __module_register_func(dict, keys),
    __module_register_func(dict, values),
    __module_register_func(dict, get),
    __module_register_func(dict, get_path),
    __module_register_func(dict, exists),
    __module_register_func(dict, contains),
    __module_register_func(dict, size),
    __module_register_func(dict, is_empty),
} __module_class_end(dict)

}  // namespace dict
}  // namespace mdl
}  // namespace jas
