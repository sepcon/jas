#include "VarManipModuleShared.h"
#include "jas/FunctionModuleBaseT.h"

namespace jas {
namespace mdl {
namespace list {
using std::begin;
using std::end;
using std::make_shared;

#define __throw_if_param_null(param)                 \
  __jas_ni_func_throw_invalidargs_if(param.isNull(), \
                                     "List function param must not be null");

#define __list_throw_if_not_is_list(thelist)                         \
  __jas_ni_func_throw_invalidargs_if(thelist.isNull(),               \
                                     "The list must  not be null "); \
  __jas_func_throw_invalidargs_if(!thelist.isList(), "expect a list", thelist)

#define __list_check_standard_params(params) \
  __list_throw_if_not_is_list(params);       \
  __throw_if_param_null(params);             \
  __jas_ni_func_throw_invalidargs_if(params.size() < 2, "Missing param")

#define __list_func(name, params) __module_func(list, name, params)

__list_func(append, params) {
  __list_check_standard_params(params);
  auto &lst = detach_ref(params[0]);
  auto size = params.size();
  for (size_t i = 1; i < size; ++i) {
    lst.add(params.at(i));
  }
  return lst;
}

__list_func(extend, params) {
  __list_check_standard_params(params);
  auto &theList = detach_ref(params[0]).asList();
  auto size = params.size();
  for (size_t i = 1; i < size; ++i) {
    if (params[i].isList()) {
      auto &tbAppend = params[i].asList();
      theList.insert(end(theList), begin(tbAppend), end(tbAppend));
    } else {
      theList.push_back(params.at(i));
    }
  }
  return params[0];
}

__list_func(remove, params) {
  __list_check_standard_params(params);
  auto &theList = detach_ref(params[0]).asList();
  auto &tbRemoved = params[1];
  auto itRemoved = std::remove(begin(theList), end(theList), tbRemoved);
  if (itRemoved != end(theList)) {
    theList.erase(itRemoved);
    return true;
  } else {
    return false;
  }
}

__list_func(pop, params) {
  __list_check_standard_params(params);
  __jas_func_throw_invalidargs_if(!params[1].isNumber(),
                                  "Expect index to be removed with NUMBER type",
                                  params[1]);

  auto rmPos = params[1].getValue<size_t>();
  __jas_func_throw_invalidargs_if(
      rmPos >= params[0].size(),
      strJoin("Out of range access, list size is ", params[0].size()),
      params[1]);

  auto &theList = detach_ref(params[0]).asList();
  auto rmItem = std::move(theList[rmPos]);
  theList.erase(std::begin(theList) + rmPos);
  return rmItem;
}

__list_func(insert, params) {
  __list_check_standard_params(params);
  __jas_ni_func_throw_invalidargs_if(
      params.size() != 3, "Expect 3 arguments [$thelist, insertPos, value]");
  auto &internalList = detach_ref(params[0]).asList();
  auto &insertPos = params[1];
  __jas_func_throw_invalidargs_if(
      !insertPos.isNumber() ||
          insertPos.getValue<size_t>() > internalList.size(),
      "Expect insert position is an integer and less than the list.size",
      insertPos);

  internalList.insert(internalList.begin() + insertPos.getValue<size_t>(),
                      params[2]);
  return true;
}

__list_func(len, varList) {
  __list_throw_if_not_is_list(varList);
  return varList.size();
}
__list_func(count, params) {
  __list_check_standard_params(params);
  auto &theList = params[0].asList();
  return std::count(begin(theList), end(theList), params[1]);
}

__list_func(sort, varList) {
  __list_throw_if_not_is_list(varList);
  auto &internalList = detach_ref(varList).asList();
  std::sort(begin(internalList), end(internalList));
  return varList;
}
__list_func(unique, input) {
  __list_throw_if_not_is_list(input);
  auto &theList = detach_ref(input).asList();
  theList.erase(std::unique(begin(theList), end(theList)), end(theList));
  return input;
}

__module_class_begin(list){
    __module_register_func(list, append), __module_register_func(list, extend),
    __module_register_func(list, remove), __module_register_func(list, insert),
    __module_register_func(list, sort),   __module_register_func(list, count),
    __module_register_func(list, unique), __module_register_func(list, len),
    __module_register_func(list, pop),
} __module_class_end(list)

}  // namespace list
}  // namespace mdl
}  // namespace jas
