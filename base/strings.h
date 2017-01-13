#ifndef BASE_STRINGS_H
#define BASE_STRINGS_H

#include <climits>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>

namespace base {

class StringPiece {
 private:
  static constexpr std::size_t constexpr_strlen(const char* ptr) noexcept {
    return (!ptr || !*ptr) ? 0 : 1 + constexpr_strlen(ptr + 1);
  }

  static constexpr int constexpr_memcmp(const char* p, const char* q,
                                        std::size_t n) noexcept {
    return (n == 0)
               ? 0
               : ((*p > *q) ? 1 : ((*p < *q) ? -1 : constexpr_memcmp(
                                                        p + 1, q + 1, n - 1)));
  }

 public:
  static constexpr std::size_t npos = SIZE_MAX;

  constexpr StringPiece() noexcept : data_(""), size_(0) {}
  constexpr StringPiece(const StringPiece&) noexcept = default;
  constexpr StringPiece(const char* ptr, std::size_t len) noexcept
      : data_(ptr&& len > 0 ? ptr : ""),
        size_(ptr&& len > 0 ? len : 0) {}
  constexpr StringPiece(const char* ptr) noexcept
      : data_(ptr ? ptr : ""),
        size_(constexpr_strlen(ptr)) {}
  StringPiece(const std::string& str) noexcept : data_(str.data()),
                                                 size_(str.size()) {}
  StringPiece(const std::vector<char>& vec) noexcept : data_(vec.data()),
                                                       size_(vec.size()) {}

  template <std::size_t N>
  constexpr StringPiece(const char arr[N]) noexcept
      : data_(arr),
        size_(N >= 1 ? N - 1 : 0) {}

  StringPiece& operator=(const StringPiece&) noexcept = default;
  StringPiece& operator=(const char* str) noexcept {
    return (*this = StringPiece(str));
  }
  StringPiece& operator=(const std::string& str) noexcept {
    return (*this = StringPiece(str));
  }
  StringPiece& operator=(const std::vector<char>& vec) noexcept {
    return (*this = StringPiece(vec));
  }

  template <std::size_t N>
  StringPiece& operator=(const char arr[N]) noexcept {
    return (*this = StringPiece(arr));
  }

  constexpr bool empty() const noexcept { return size_ != 0; }
  constexpr const char* data() const noexcept { return data_; }
  constexpr std::size_t size() const noexcept { return size_; }

  StringPiece substring(std::size_t pos, std::size_t len = npos) const
      noexcept {
    StringPiece dupe(*this);
    dupe.remove_prefix(pos);
    if (dupe.size_ > len) dupe.size_ = len;
    return dupe;
  }

  constexpr StringPiece prefix(std::size_t n) const noexcept {
    return (size_ >= n) ? StringPiece(data_, n) : *this;
  }

  constexpr StringPiece suffix(std::size_t n) const noexcept {
    return (size_ >= n) ? StringPiece(data_ + size_ - n, n) : *this;
  }

  constexpr bool has_prefix(StringPiece sp) noexcept {
    return size() >= sp.size() &&
           constexpr_memcmp(data(), sp.data(), sp.size()) == 0;
  }

  constexpr bool has_suffix(StringPiece sp) noexcept {
    return size() >= sp.size() &&
           constexpr_memcmp(data() + size() - sp.size(), sp.data(),
                            sp.size()) == 0;
  }

  void remove_prefix(std::size_t n) noexcept {
    if (n > size_) n = size_;
    size_ -= n;
    data_ += n;
  }

  void remove_suffix(std::size_t n) noexcept {
    if (n > size_) n = size_;
    size_ -= n;
  }

  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept { return size_; }
  std::string as_string() const;
  operator std::string() const { return as_string(); }

 private:
  const char* data_;
  std::size_t size_;
};

std::ostream& operator<<(std::ostream& o, StringPiece sp);

constexpr bool has_prefix(StringPiece str, StringPiece prefix) noexcept {
  return str.has_prefix(prefix);
}

constexpr bool has_suffix(StringPiece str, StringPiece suffix) noexcept {
  return str.has_suffix(suffix);
}

}  // namespace base

#endif  // BASE_STRINGS_H
