#include "jas/ConsoleLogger.h"
#include "jas/Var.h"
#include "jas/ObjectPath.h"

using namespace jas;
using namespace std::string_view_literals;

int main() {
  Var var;

  //  auto con = Var::object({{"con", 1}});
  //  auto world = Var::object({{"world", con}});
  //  Var hello = Var::object({{"hello", world}});
  //  CLoggerTimerSection measure{"Start measuring"};
  //  for (auto i = 0; i < 1000000; ++i) {
  //    hello.exists("hello/world/con");
  //    hello.getPath("hello/world/con");
  //  }
  clogger() << var.toJson();
  return 0;
}
