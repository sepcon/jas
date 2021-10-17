#pragma once
#include <typeindex>

#include "Var.h"

namespace jas {

inline const CharType* nameOfType(std::type_index typeidx) {
  static std::map<std::type_index, const CharType*> type2name = {
      {typeid(String), JASSTR("String")},
      {typeid(Var::Dict), JASSTR("Dict")},
      {typeid(Var::List), JASSTR("List")},
      {typeid(Var), JASSTR("Variant")},
      {typeid(bool), JASSTR("Boolean")},
      {typeid(double), JASSTR("Real number")},
      {typeid(int), JASSTR("Integer")},
      {typeid(Number), JASSTR("Number")},
      {typeid(Var::Ref), JASSTR("Ref")},
  };

  if (auto it = type2name.find(typeidx); it != std::end(type2name)) {
    return it->second;
  } else {
    return JASSTR("Unknown type");
  }
}

template <class T>
const CharType* nameOfType() {
  return nameOfType(typeid(std::decay_t<T>));
}

template <class T>
const CharType* typeNameOf(T&& val) {
  return nameOfType(typeid(std::decay_t<decltype(val)>));
}

}  // namespace jas
