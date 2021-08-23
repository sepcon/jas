#include "jas/EvaluatedValue.h"

namespace jas {
using std::make_shared;
using std::move;

EvaluatedValue::EvaluatedValue(List arr) : value(std::move(arr)) {}
EvaluatedValue::EvaluatedValue(CList arr)
    : value(make_shared<CList>(std::move(arr))) {}
EvaluatedValue::EvaluatedValue(Map obj) : value(std::move(obj)) {}
EvaluatedValue::EvaluatedValue(CMap obj)
    : value(make_shared<CMap>(std::move(obj))) {}
EvaluatedValue::EvaluatedValue(Single Single) : value(std::move(Single)) {}
EvaluatedValue::EvaluatedValue(CSingle Single)
    : value(make_shared<CSingle>(std::move(Single))) {}

Json EvaluatedValue::toJson() const {
  if (isMap()) {
    auto out = JsonTrait::object();
    if (auto &obj = getMap()) {
      for (auto &[key, val] : *obj) {
        JsonTrait::add(out, key, val.toJson());
      }
    }
    return out;
  } else if (isList()) {
    auto out = JsonTrait::array();
    if (auto &obj = getList()) {
      for (auto &val : *obj) {
        JsonTrait::add(out, val.toJson());
      }
    }
    return out;
  } else {
    auto &Single = getSingle();
    return Single ? Single->value : Json{};
  }
}

size_t EvaluatedValue::size() const {
  if (isList()) {
    return getList()->size();
  } else if (isMap()) {
    return getMap()->size();
  } else {
    assert(false);
    return 0;
  }
}

void EvaluatedValue::add(EvaluatedValue val) {
  assert(isList());
  getList()->push_back(move(val));
}

void EvaluatedValue::add(String key, EvaluatedValue val) {
  assert(isMap());
  (*getMap())[move(key)] = (move(val));
}

EvaluatedValue &EvaluatedValue::at(size_t i) {
  assert(isList());
  return getList()->at(i);
}

EvaluatedValue &EvaluatedValue::at(const String &key) {
  assert(isMap());
  return getMap()->at(key);
}

EvaluatedValue &EvaluatedValue::at(size_t i) const {
  assert(isList());
  return getList()->at(i);
}

EvaluatedValue &EvaluatedValue::at(const String &key) const {
  assert(isMap());
  return getMap()->at(key);
}

size_t EvaluatedValue::erase(size_t i) {
  assert(isList());
  if (getList()->size() <= i) {
    return 0;
  } else {
    getList()->erase((getList()->begin() + i));
    return 1;
  }
}

size_t EvaluatedValue::erase(const String &key) {
  assert(isMap());
  return getMap()->erase(key);
}

EvaluatedValue EvaluatedValue::list(CList arr) {
  return make_shared<CList>(std::move(arr));
}

EvaluatedValue EvaluatedValue::map(CMap obj) {
  return make_shared<CMap>(std::move(obj));
}
EvaluatedValue EvaluatedValue::single(CSingle Single) {
  return make_shared<CSingle>(std::move(Single));
}

}  // namespace jas
