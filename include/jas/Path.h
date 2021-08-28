#pragma once

#include "Exception.h"
#include "String.h"

namespace jas {
namespace __details {

template <class _String>
struct DefaultPathValidator {
  bool operator()(const _String& /*str*/, size_t /*currentPos*/,
                  size_t /*startPos*/) const {
    return true;
  }
};

__mc_jas_exception(InvalidPathException);
template <class _StringType, CharType _Sep = JASSTR('/'),
          class _Validator = DefaultPathValidator<_StringType>>
class PathBase {
 public:
  using StringType = _StringType;
  using Validator = _Validator;

  static const CharType seperator = _Sep;

  struct Iterator : public std::forward_iterator_tag {
    Iterator(const PathBase* owner = nullptr) : owner_(owner) {
      if (owner) {
        view_ = nextView(owner->str_, 0);
      }
    }

    Iterator& operator++() {
      view_ = nextView(owner_->str_,
                       (view_.data() - owner_->str_.data() + 1 + view_.size()));
      if (view_.empty()) {
        owner_ = nullptr;
      }
      return *this;
    }

    Iterator operator++(int) {
      auto it = *this;
      this->operator++();
      return it;
    }

    bool operator==(const Iterator& other) const {
      return owner_ == other.owner_ && view_ == other.view_;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    const StringView& operator*() const { return view_; }

    static StringView nextView(const PathBase::StringType& str, size_t start) {
      Validator isCharValid;
      auto i = start;
      auto size = str.size();
      while (i < size) {
        if (str[i] != _Sep) {
          if (!isCharValid(str, i, start)) {
            __jas_throw(InvalidPathException, "Invalid character '", str[i],
                        "'");
          }
          ++i;
        } else if (i < start + 1) {
          start = ++i;
        } else {
          return StringView{&(str[start]), static_cast<size_t>(i - start)};
        }
      }
      if (start < size) {
        return StringView{&(str[start]), static_cast<size_t>(size - start)};
      }
      return {};
    }
    const PathBase* owner_ = nullptr;
    StringView view_;
  };

  PathBase() = default;
  PathBase(const StringType& str) : str_(str) {}
  PathBase(StringType&& str) : str_(std::move(str)) {}

  template <typename _String,
            std::enable_if_t<std::is_constructible_v<StringType, _String>,
                             bool> = true>
  PathBase(_String&& str) : str_(std::forward<_String>(str)) {}

  const StringType& underType() const { return this->str_; }
  operator const StringType&() const { return this->str_; }

  auto begin() { return Iterator{this}; }
  auto end() { return Iterator{}; }
  auto begin() const { return Iterator{this}; }
  auto end() const { return Iterator{}; }
  auto cbegin() const { return begin(); }
  auto cend() const { return end(); }
  size_t size() const { return str_.size(); }
  bool empty() const { return str_.empty(); }

 protected:
  StringType str_;
};

template <CharType _Sep = JASSTR('/'),
          class _Validator = DefaultPathValidator<String>>
class BasicPath : public PathBase<String, _Sep, _Validator> {
  using _Base = PathBase<String, _Sep, _Validator>;

 public:
  using _Base::_Base;

  operator StringView() const { return StringView{this->str_}; }
  template <class _String>
  void operator/=(_String&& str) {
    join(*this, std::forward<_String>(str));
  }

  template <class _String>
  BasicPath operator/(_String&& str) const {
    BasicPath path = *this;
    join(path, std::forward<_String>(str));
    return path;
  }

  template <class _String>
  void operator+=(_String&& str) {
    if (this->str_.empty()) {
      str = std::forward<_String>(str);
    } else {
      str += std::forward<_String>(str);
    }
  }

  template <class _String>
  void join(BasicPath& path, _String&& str) {
    if (path.str_.empty()) {
      path.str_ = std::forward<_String>(str);
    } else if (path.str_.back() == _Sep) {
      path.str_ += str;
    } else {
      path.str_ += _Sep;
      path.str_ += str;
    }
  }
};

template <CharType _Sep = JASSTR('/'),
          class _Validator = DefaultPathValidator<StringView>>
class BasicPathView : public PathBase<StringView, _Sep, _Validator> {
  using _Base = PathBase<StringView, _Sep, _Validator>;

 public:
  using _Base::_Base;
  BasicPathView(const BasicPath<_Sep>& path) : _Base(path.underType()) {}
};

}  // namespace __details

using __details::BasicPath;
using __details::BasicPathView;
using __details::DefaultPathValidator;
using Path = BasicPath<>;
using PathView = BasicPathView<>;

}  // namespace jas
