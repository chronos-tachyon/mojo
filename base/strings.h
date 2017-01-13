// base/strings.h - StringPiece and other string helpers
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_STRINGS_H
#define BASE_STRINGS_H

#include <climits>
#include <cstring>
#include <iosfwd>
#include <iterator>
#include <string>
#include <vector>

#include "external/com_googlesource_code_re2/re2/stringpiece.h"

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
  static constexpr size_type partial_strlen(const_pointer ptr,
                                            size_type sum) noexcept {
    return !*ptr ? sum : partial_strlen(ptr + 1, sum + 1);
  }
  static constexpr size_type ce_strlen(const_pointer ptr) noexcept {
    return !ptr ? 0 : partial_strlen(ptr, 0);
  }

  static constexpr int ce_memcmp(const_pointer p, const_pointer q,
                                 size_type n) noexcept {
    return (n == 0)
               ? 0
               : ((*p < *q) ? -1
                            : ((*p > *q) ? 1 : ce_memcmp(p + 1, q + 1, n - 1)));
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
    return (n != 0)
               ? n
               : ((a.size() < b.size()) ? -1 : ((a.size() > b.size()) ? 1 : 0));
  }

 public:
  static constexpr size_type npos = SIZE_MAX;

  // StringPiece is default constructible, copyable, and moveable.
  // Move is indistinguishable from copy.
  constexpr StringPiece() noexcept : data_(nullptr), size_(0) {}
  constexpr StringPiece(const StringPiece&) noexcept = default;
  constexpr StringPiece(StringPiece&&) noexcept = default;
  StringPiece& operator=(const StringPiece&) noexcept = default;
  StringPiece& operator=(StringPiece&&) noexcept = default;

  // StringPiece is constructible from a pointer and a length.
  constexpr StringPiece(const_pointer ptr, size_type len) noexcept
      : data_(ptr),
        size_(len) {}

  // StringPiece is constructible from a C string.
  constexpr StringPiece(const_pointer ptr) noexcept
      : StringPiece(ptr, ce_strlen(ptr)) {}

  // StringPiece is constructible from a std::string.
  StringPiece(const std::string& str) noexcept
      : StringPiece(str.data(), str.size()) {}

  // StringPiece is constructible from a std::vector<char>.
  StringPiece(const std::vector<char>& vec) noexcept
      : StringPiece(vec.data(), vec.size()) {}

  // StringPiece is constructible from a string constant.
  template <std::size_t N>
  constexpr StringPiece(const char arr[N]) noexcept
      : StringPiece(arr, N >= 1 ? N - 1 : 0) {}

  // StringPiece is constructible from an re2::StringPiece.
  StringPiece(re2::StringPiece sp) noexcept
      : StringPiece(sp.data(), sp.size()) {}

  constexpr bool empty() const noexcept { return size_ == 0; }
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

  constexpr int compare(StringPiece other) const noexcept {
    return final_compare(*this, other, partial_compare(*this, other));
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

  constexpr bool has_prefix(StringPiece sp) const noexcept {
    return size_ >= sp.size_ && ce_memcmp(data_, sp.data_, sp.size_) == 0;
  }

  constexpr bool has_suffix(StringPiece sp) const noexcept {
    return size_ >= sp.size_ &&
           ce_memcmp(data_ + size_ - sp.size_, sp.data_, sp.size_) == 0;
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

  bool remove_prefix(StringPiece sp) noexcept {
    if (!has_prefix(sp)) return false;
    remove_prefix(sp.size());
    return true;
  }

  bool remove_suffix(StringPiece sp) noexcept {
    if (!has_suffix(sp)) return false;
    remove_suffix(sp.size());
    return true;
  }

  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept { return size_; }
  std::string as_string() const { return std::string(data_, size_); }
  operator std::string() const { return as_string(); }
  operator re2::StringPiece() const noexcept {
    return re2::StringPiece(data_, size_);
  }

 private:
  const_pointer data_;
  size_type size_;
};

std::ostream& operator<<(std::ostream& o, StringPiece sp);

constexpr int compare(StringPiece a, StringPiece b) noexcept {
  return a.compare(b);
}

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

constexpr StringPiece substring(
    StringPiece sp, StringPiece::size_type pos,
    StringPiece::size_type len = StringPiece::npos) noexcept {
  return sp.substring(pos, len);
}

constexpr StringPiece prefix(StringPiece sp,
                             StringPiece::size_type len) noexcept {
  return sp.prefix(len);
}

constexpr StringPiece suffix(StringPiece sp,
                             StringPiece::size_type len) noexcept {
  return sp.suffix(len);
}

constexpr bool has_prefix(StringPiece sp, StringPiece prefix) noexcept {
  return sp.has_prefix(prefix);
}

constexpr bool has_suffix(StringPiece sp, StringPiece suffix) noexcept {
  return sp.has_suffix(suffix);
}

constexpr StringPiece remove_prefix(StringPiece sp,
                                    StringPiece::size_type len) noexcept {
  return sp.substring(len);
}

constexpr StringPiece remove_prefix(StringPiece sp,
                                    StringPiece prefix) noexcept {
  return sp.has_prefix(prefix) ? sp.substring(prefix.size()) : sp;
}

constexpr StringPiece remove_suffix(StringPiece sp,
                                    StringPiece::size_type len) noexcept {
  return sp.substring(0, (sp.size() >= len) ? (sp.size() - len) : 0);
}

constexpr StringPiece remove_suffix(StringPiece sp,
                                    StringPiece suffix) noexcept {
  return sp.has_suffix(suffix) ? sp.substring(0, sp.size() - suffix.size())
                               : sp;
}

}  // namespace base

#endif  // BASE_STRINGS_H
