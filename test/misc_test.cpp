#include "jas/ConsoleLogger.h"
#include "jas/Path.h"
#include "jas/Var.h"

using namespace jas;
using namespace std;
using namespace std::string_view_literals;

struct JasSymbol {
  constexpr JasSymbol(CharType prefix = 0) : prefix{prefix} {}
  bool matchPrefix(const String& str) const {
    if (!str.empty() && str[0] == prefix[0]) {
      return true;
    } else {
      return false;
    }
  }

  CharType prefix[1];
};

bool operator<(const JasSymbol& sym, const String& pair) {
  if (sym.matchPrefix(pair)) {
    return false;
  } else {
    return sym.prefix < pair;
  }
}

bool operator<(const String& str, const JasSymbol& sym) {
  if (sym.matchPrefix(str)) {
    return false;
  } else {
    return str < sym.prefix;
  }
}

template <class _Sequence>
struct RangeIterable {
  using Iterator = typename _Sequence::iterator;
  RangeIterable(Iterator b, Iterator e) : b_(b), e_(e) {}
  RangeIterable(std::pair<Iterator, Iterator> p) : b_(p.first), e_(p.second) {}

  auto begin() { return b_; }
  auto begin() const { return b_; }
  auto end() { return e_; }
  auto end() const { return e_; }
  bool empty() const { return b_ != e_; }

  Iterator b_, e_;
};
const constexpr JasSymbol symbol_var('$');
const constexpr JasSymbol symbol_op('@');

template <class _String>
struct SymbolPathValidator : public DefaultPathValidator<_String> {
  using _Base = DefaultPathValidator<_String>;
  using PrefixType = typename _String::value_type;
  SymbolPathValidator(PrefixType prefix) : prefix(prefix) {}
  bool operator()(const _String& str, size_t currentPos,
                  size_t startPos) const {
    if (!_Base::operator()(str, currentPos, startPos)) {
      return str[currentPos] == prefix && currentPos == startPos;
    }
    return true;
  }
  PrefixType prefix = '\0';
};

struct VarPathValidator : public SymbolPathValidator<StringView> {
  VarPathValidator() : SymbolPathValidator<StringView>('$') {}
};
struct OperationPathValidator : public SymbolPathValidator<StringView> {
  OperationPathValidator() : SymbolPathValidator<StringView>('@') {}
};

int main() {
  BasicPathView<':', OperationPathValidator> pathcollon =
      "::@hello:[@field:hello/]:con:::";

  for (auto it = pathcollon.begin(); it != pathcollon.end(); ++it) {
    cloginfo() << *it;
  }

  return 0;
}
