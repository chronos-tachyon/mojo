// base/strings.h - StringPiece and other string helpers
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_STRINGS_H
#define BASE_STRINGS_H

#include <climits>
#include <cstring>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

namespace base {

// StringPiece is a value type which holds a read-only view of a buffer, such
// as a (piece of a) std::string. Take a StringPiece by value where you would
// normally take a "const std::string&", especially in situations where you
// don't need a std::string.
//
// NOTE: StringPiece does not own the memory it points to.
//       Use std::string or alternatives if you need memory to persist.
//       In particular, StringPiece is rarely appropriate as an object member.
class StringPiece {
 public:
  using value_type = char;
  using const_reference = const char&;
  using const_pointer = const char*;
  using const_iterator = const char*;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reference = const_reference;
  using pointer = const_pointer;
  using iterator = const_iterator;
  using reverse_iterator = const_reverse_iterator;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

 private:
  static constexpr size_type ce_strlen(const_pointer ptr,
                                       size_type sum = 0) noexcept {
    return (!ptr || !*ptr) ? sum : ce_strlen(ptr + 1, sum + 1);
  }

  template <typename T>
  static constexpr int cmp(T a, T b) noexcept {
    return ((a > b) ? 1 : ((a < b) ? -1 : 0));
  }

  static constexpr int chain(int m, int n) noexcept { return (m != 0) ? m : n; }

  static constexpr int ce_memcmp(const_pointer p, const_pointer q,
                                 size_type n) noexcept {
    return (n == 0) ? 0 : chain(cmp(*p, *q), ce_memcmp(p + 1, q + 1, n - 1));
  }

  template <typename T>
  static constexpr T ce_min(T a, T b) noexcept {
    return (a < b) ? a : b;
  }

  static constexpr int partial_compare(StringPiece a, StringPiece b) noexcept {
    return ce_memcmp(a.data(), b.data(), ce_min(a.size(), b.size()));
  }

  static constexpr int final_compare(StringPiece a, StringPiece b,
                                     int n) noexcept {
    return chain(n, cmp(a.size(), b.size()));
  }

 public:
  static constexpr size_type npos = SIZE_MAX;

  // StringPiece is default constructible, copyable, and moveable.
  // Move is indistinguishable from copy.
  constexpr StringPiece() noexcept : data_(""), size_(0) {}
  constexpr StringPiece(const StringPiece&) noexcept = default;
  constexpr StringPiece(StringPiece&&) noexcept = default;
  StringPiece& operator=(const StringPiece&) noexcept = default;
  StringPiece& operator=(StringPiece&&) noexcept = default;

  // StringPiece is constructible from a pointer and a length.
  constexpr StringPiece(const_pointer ptr, size_type len) noexcept
      : data_(ptr&& len > 0 ? ptr : ""),
        size_(ptr&& len > 0 ? len : 0) {}

  // StringPiece is constructible from a C string.
  constexpr StringPiece(const_pointer ptr) noexcept : data_(ptr ? ptr : ""),
                                                      size_(ce_strlen(ptr)) {}

  // StringPiece is constructible from a std::string.
  StringPiece(const std::string& str) noexcept : data_(str.data()),
                                                 size_(str.size()) {}

  // StringPiece is constructible from a std::vector<char>.
  StringPiece(const std::vector<char>& vec) noexcept : data_(vec.data()),
                                                       size_(vec.size()) {}

  // StringPiece is constructible from a string constant.
  template <std::size_t N>
  constexpr StringPiece(const char arr[N]) noexcept
      : data_(arr),
        size_(N >= 1 ? N - 1 : 0) {}

  constexpr bool empty() const noexcept { return size_ != 0; }
  constexpr const_pointer data() const noexcept { return data_; }
  constexpr size_type size() const noexcept { return size_; }

  constexpr const_iterator begin() const noexcept { return data_; }
  constexpr const_iterator cbegin() const noexcept { return data_; }
  constexpr const_iterator end() const noexcept { return data_ + size_; }
  constexpr const_iterator cend() const noexcept { return data_ + size_; }

  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(data_ + size_);
  }
  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(data_);
  }
  const_reverse_iterator crbegin() const noexcept { return rbegin(); }
  const_reverse_iterator crend() const noexcept { return rend(); }

  constexpr const_reference front() const noexcept { return *data_; }
  constexpr const_reference back() const noexcept {
    return *(data_ + size_ - 1);
  }
  constexpr const_reference operator[](size_type i) const noexcept {
    return data_[i];
  }

  constexpr StringPiece substring(size_type pos, size_type len = npos) const
      noexcept {
    return (pos >= size_)
               ? StringPiece(data_ + size_, 0)
               : ((len >= size_ - pos) ? StringPiece(data_ + pos, size_ - pos)
                                       : StringPiece(data_ + pos, len));
  }

  constexpr StringPiece prefix(size_type n) const noexcept {
    return (size_ >= n) ? StringPiece(data_, n) : *this;
  }

  constexpr StringPiece suffix(size_type n) const noexcept {
    return (size_ >= n) ? StringPiece(data_ + size_ - n, n) : *this;
  }

  constexpr bool has_prefix(StringPiece sp) noexcept {
    return size() >= sp.size() && ce_memcmp(data(), sp.data(), sp.size()) == 0;
  }

  constexpr bool has_suffix(StringPiece sp) noexcept {
    return size() >= sp.size() &&
           ce_memcmp(data() + size() - sp.size(), sp.data(), sp.size()) == 0;
  }

  void remove_prefix(size_type n) noexcept {
    if (n > size_) n = size_;
    size_ -= n;
    data_ += n;
  }

  void remove_suffix(size_type n) noexcept {
    if (n > size_) n = size_;
    size_ -= n;
  }

  friend constexpr int compare(StringPiece a, StringPiece b) noexcept {
    return final_compare(a, b, partial_compare(a, b));
  }

  constexpr int compare(StringPiece other) noexcept {
    return final_compare(*this, other, partial_compare(*this, other));
  }

  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept { return size_; }
  std::string as_string() const { return std::string(data_, size_); }
  operator std::string() const { return as_string(); }

 private:
  const_pointer data_;
  size_type size_;
};

std::ostream& operator<<(std::ostream& o, StringPiece sp);

constexpr bool operator==(StringPiece a, StringPiece b) noexcept {
  return compare(a, b) == 0;
}
constexpr bool operator!=(StringPiece a, StringPiece b) noexcept {
  return !(a == b);
}
constexpr bool operator<(StringPiece a, StringPiece b) noexcept {
  return compare(a, b) < 0;
}
constexpr bool operator>(StringPiece a, StringPiece b) noexcept {
  return (b < a);
}
constexpr bool operator<=(StringPiece a, StringPiece b) noexcept {
  return !(b < a);
}
constexpr bool operator>=(StringPiece a, StringPiece b) noexcept {
  return !(a < b);
}

constexpr bool has_prefix(StringPiece str, StringPiece prefix) noexcept {
  return str.has_prefix(prefix);
}

constexpr bool has_suffix(StringPiece str, StringPiece suffix) noexcept {
  return str.has_suffix(suffix);
}

constexpr StringPiece substring(
    StringPiece str, StringPiece::size_type pos,
    StringPiece::size_type len = StringPiece::npos) noexcept {
  return str.substring(pos, len);
}

}  // namespace base

#endif  // BASE_STRINGS_H
