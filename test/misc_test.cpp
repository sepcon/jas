#include "jas/ConsoleLogger.h"
#include "jas/EvaluatedValue.h"

using namespace jas;
template <class T>
auto ev(T&& v) {
  return EvaluatedValue{v};
}

int main() {
  auto arr = EvaluatedValue::list();
  arr.add(ev(10));
  arr.add(ev(10));
  arr.add(ev(10));
  clogger() << "isArray = " << arr.isList();
  clogger() << "isItem = " << arr.isSingle();
  clogger() << arr.toJson();

  arr = EvaluatedValue::map({
      {"hello", ev(10)},
      {"world", ev("Hello")},
  });

  arr.add("world1", EvaluatedValue::list({ev(1), ev(1), ev(1), ev(1)}));
  clogger() << "isArray = " << arr.isList();
  clogger() << "isItem = " << arr.isSingle();
  clogger() << "isMap = " << arr.isMap();
  clogger() << arr.toJson();
}
