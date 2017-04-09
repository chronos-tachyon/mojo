// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_CHARS_H
#define BASE_CHARS_H

#include <iosfwd>

#include "base/bytes.h"
#include "external/com_googlesource_code_re2/re2/stringpiece.h"

namespace base {

namespace internal {

inline std::size_t hash_chars(const char* ptr, std::size_t len) noexcept {
  return hash_bytes(reinterpret_cast<const uint8_t*>(ptr), len);
}

void append_chars(std::string* out, const char* ptr, std::size_t len);

inline constexpr std::size_t ce_strlen_partial(const char* ptr,
                                               std::size_t sum) noexcept {
  return !*ptr ? sum : ce_strlen_partial(ptr + 1, sum + 1);
}

inline constexpr std::size_t ce_strlen(const char* ptr) noexcept {
  return !ptr ? 0 : ce_strlen_partial(ptr, 0);
}

inline constexpr bool ce_contains(char ch, const char* set,
                                  std::size_t len) noexcept {
  return (len > 0) && ((ch == *set) || ce_contains(ch, set + 1, len - 1));
}

}  // namespace internal

namespace charmatch {

struct is_exactly {
  char value;

  constexpr is_exactly(char v) noexcept : value(v) {}
  constexpr bool operator()(char ch) const noexcept { return ch == value; }
};

struct is_oneof {
  const char* set;
  std::size_t len;

  constexpr is_oneof(const char* s, std::size_t l) noexcept : set(s), len(l) {}
  constexpr is_oneof(const char* s) noexcept
      : is_oneof(s, base::internal::ce_strlen(s)) {}
  constexpr bool operator()(char ch) const noexcept {
    return base::internal::ce_contains(ch, set, len);
  }
};

struct is_whitespace {
  constexpr is_whitespace() noexcept = default;
  constexpr bool operator()(char ch) const noexcept {
    return ch == ' ' || ch == '\t' || (ch >= '\n' && ch <= '\r');
  }
};

struct is_eol {
  constexpr is_eol() noexcept = default;
  constexpr bool operator()(char ch) const noexcept {
    return ch == '\n' || ch == '\r';
  }
};

struct is_nul {
  constexpr is_nul() noexcept = default;
  constexpr bool operator()(char ch) const noexcept { return !ch; }
};

}  // namespace charmatch

template <bool Mutable>
class BasicChars;

// Represents a view into an immutable character buffer.
//
// NOTE: Chars does not own the memory it points to.
//       Use std::string or alternatives if you need memory to persist.
//       In particular, Chars is rarely appropriate as an object member.
//
using Chars = BasicChars<false>;

// Represents a view into a mutable character buffer.
//
// NOTE: MutableChars does not own the memory it points to.
//       Use std::string or alternatives if you need memory to persist.
//       In particular, MutableChars is rarely appropriate as an object member.
//
using MutableChars = BasicChars<true>;

template <bool Mutable>
class BasicChars {
 private:
  template <bool B, typename T = void>
  using If = typename std::enable_if<B, T>::type;

  template <bool B, typename T, typename U>
  using Cond = typename std::conditional<B, T, U>::type;

  template <typename Predicate, typename T = decltype(std::declval<Predicate>()(
                                    std::declval<char>()))>
  using is_predicate = std::is_convertible<T, bool>;

  template <typename Predicate>
  using predicate_t = If<is_predicate<Predicate>::value, Predicate>;

  template <typename Container, typename T = decltype(std::declval<Container&>().data())>
  using is_container = std::is_convertible<T, const char*>;

  template <typename Container>
  using container_t = If<is_container<Container>::value, Container>;

  template <typename Container, typename T = decltype(std::declval<Container&>().data())>
  using is_byte_container = std::is_same<T, const uint8_t*>;

  template <typename Container>
  using byte_container_t = If<is_byte_container<Container>::value, Container>;

 public:
  using value_type = char;
  using const_reference = const char&;
  using const_pointer = const char*;
  using mutable_reference = char&;
  using mutable_pointer = char*;
  using reference = Cond<Mutable, mutable_reference, const_reference>;
  using pointer = Cond<Mutable, mutable_pointer, const_pointer>;

  using const_iterator = const_pointer;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using iterator = pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;

  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  template <size_type N>
  using carray_reference = Cond<Mutable, char (&)[N], const char (&)[N]>;

  template <size_type N>
  using array_reference =
      Cond<Mutable, std::array<char, N>&, const std::array<char, N>&>;

  using vector_reference =
      Cond<Mutable, std::vector<char>&, const std::vector<char>&>;

  using byte_pointer = Cond<Mutable, uint8_t*, const uint8_t*>;

  template <size_type N>
  using byte_carray_reference =
      Cond<Mutable, uint8_t (&)[N], const uint8_t (&)[N]>;

  template <size_type N>
  using byte_array_reference =
      Cond<Mutable, std::array<uint8_t, N>&, const std::array<uint8_t, N>&>;

  using byte_vector_reference =
      Cond<Mutable, std::vector<uint8_t>&, const std::vector<uint8_t>&>;

  static constexpr size_type npos = SIZE_MAX;

  constexpr BasicChars(std::nullptr_t = nullptr, size_type = 0) noexcept
      : data_(nullptr),
        size_(0) {}

  constexpr BasicChars(pointer ptr, size_type len) noexcept : data_(ptr),
                                                              size_(len) {}

  constexpr BasicChars(pointer ptr) noexcept
      : data_(ptr),
        size_(base::internal::ce_strlen(ptr)) {}

  template <size_type N>
  constexpr BasicChars(carray_reference<N> a) noexcept : data_(a), size_(N) {}

  template <size_type N>
  constexpr BasicChars(array_reference<N> a) noexcept : data_(a.data()),
                                                        size_(N) {}

  BasicChars(vector_reference v) : data_(v.data()), size_(v.size()) {}

  constexpr explicit BasicChars(byte_pointer ptr, size_type len) noexcept
      : data_(reinterpret_cast<pointer>(ptr)),
        size_(len) {}

  constexpr explicit BasicChars(BasicBytes<Mutable> b) noexcept
      : data_(reinterpret_cast<pointer>(b.data())),
        size_(b.size()) {}

  template <size_type N>
  constexpr explicit BasicChars(byte_carray_reference<N> a) noexcept
      : data_(reinterpret_cast<pointer>(a)),
        size_(N) {}

  template <size_type N>
  constexpr explicit BasicChars(byte_array_reference<N> a) noexcept
      : data_(reinterpret_cast<pointer>(a.data())),
        size_(N) {}

  explicit BasicChars(byte_vector_reference v) noexcept : data_(v.data()),
                                                          size_(v.size()) {}

  template <bool Dummy = true>
  constexpr explicit BasicChars(If<Dummy && !Mutable, MutableBytes> b) noexcept
      : BasicChars(b.data(), b.size()) {}

  template <bool Dummy = true>
  BasicChars(If<Dummy && !Mutable, const std::string&> s) noexcept
      : BasicChars(s.data(), s.size()) {}

  template <bool Dummy = true>
  BasicChars(If<Dummy && !Mutable, re2::StringPiece> s) noexcept
      : BasicChars(s.data(), s.size()) {}

  constexpr BasicChars(const BasicChars&) noexcept = default;
  constexpr BasicChars(BasicChars&&) noexcept = default;

  BasicChars& operator=(const BasicChars&) noexcept = default;
  BasicChars& operator=(BasicChars&&) noexcept = default;

  BasicChars& operator=(pointer ptr) noexcept {
    data_ = ptr;
    size_ = base::internal::ce_strlen(ptr);
    return *this;
  }

  template <typename Container, typename = container_t<Container>>
  BasicChars& operator=(const Container& c) noexcept {
    data_ = c.data();
    size_ = c.size();
    return *this;
  }

  template <typename Container, typename = void, typename = byte_container_t<Container>>
  BasicChars& operator=(const Container& c) noexcept {
    data_ = reinterpret_cast<pointer>(c.data());
    size_ = c.size();
    return *this;
  }

  template <typename... Args>
  void assign(Args&&... args) noexcept {
    *this = BasicChars(std::forward<Args>(args)...);
  }

  constexpr bool empty() const noexcept { return size_ == 0; }
  constexpr pointer data() const noexcept { return data_; }
  constexpr size_type size() const noexcept { return size_; }

  constexpr byte_pointer bytes() const noexcept {
    return reinterpret_cast<byte_pointer>(data());
  }

  constexpr iterator begin() const noexcept { return data_; }
  constexpr iterator end() const noexcept { return data_ + size_; }

  constexpr const_iterator cbegin() const noexcept { return begin(); }
  constexpr const_iterator cend() const noexcept { return end(); }

  reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
  reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

  const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(cend());
  }
  const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(cbegin());
  }

  constexpr reference front() const noexcept { return *data_; }
  constexpr reference back() const noexcept { return data_[size_ - 1]; }
  constexpr reference operator[](size_type i) const noexcept {
    return data_[i];
  }

  template <typename T>
  constexpr int compare(const T& other) const noexcept {
    return internal::ce_compare(data(), other.data(), size(), other.size());
  }

  constexpr BasicChars substring(size_type pos, size_type len = npos) {
    return (pos >= size_)
               ? BasicChars(data_ + size_, 0)
               : ((len > size_ - pos) ? BasicChars(data_ + pos, size_ - pos)
                                      : BasicChars(data_ + pos, len));
  }
  constexpr BasicChars substr(size_type pos, size_type len = npos) {
    return substring(pos, len);
  }

  constexpr BasicChars prefix(size_type n) const noexcept {
    return (size_ >= n) ? BasicChars(data_, n) : *this;
  }
  constexpr BasicChars suffix(size_type n) const noexcept {
    return (size_ >= n) ? BasicChars(data_ + size_ - n, n) : *this;
  }

  constexpr bool has_prefix(Chars pre) const noexcept {
    return size_ >= pre.size_ &&
           base::internal::ce_memeq(data_, pre.data_, pre.size_);
  }
  constexpr bool has_suffix(Chars suf) const noexcept {
    return size_ >= suf.size_ &&
           base::internal::ce_memeq(data_ + size_ - suf.size_, suf.data_,
                                    suf.size_);
  }

  constexpr BasicChars strip_prefix(size_type len) const noexcept {
    return substring(len);
  }
  constexpr BasicChars strip_suffix(size_type len) const noexcept {
    return substring(0, (size_ >= len) ? (size_ - len) : 0);
  }

  constexpr BasicChars strip_prefix(Chars pre) const noexcept {
    return has_prefix(pre) ? substring(pre.size()) : *this;
  }
  constexpr BasicChars strip_suffix(Chars suf) const noexcept {
    return has_suffix(suf) ? substring(0, size_ - suf.size_) : *this;
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

  bool remove_prefix(Chars pre) noexcept {
    if (!has_prefix(pre)) return false;
    remove_prefix(pre.size());
    return true;
  }
  bool remove_suffix(Chars suf) noexcept {
    if (!has_suffix(suf)) return false;
    remove_suffix(suf.size());
    return true;
  }

  template <typename Predicate, typename = predicate_t<Predicate>>
  constexpr size_type find(Predicate pred, size_type pos = 0) const noexcept {
    return base::internal::ce_find(pred, data_, size_, pos);
  }

  constexpr size_type find(char ch, size_type pos = 0) const noexcept {
    return find(base::charmatch::is_exactly(ch), pos);
  }

  constexpr size_type find(Chars sub, size_type pos = 0) const noexcept {
    return base::internal::ce_find(sub.data_, sub.size_, data_, size_, pos);
  }

  template <typename Predicate, typename = predicate_t<Predicate>>
  constexpr size_type rfind(Predicate pred, size_type pos = npos) const
      noexcept {
    return empty() ? npos
                   : base::internal::ce_rfind(
                         pred, data_, base::internal::ce_min(pos, size_ - 1));
  }

  constexpr size_type rfind(char ch, size_type pos = npos) const noexcept {
    return rfind(base::charmatch::is_exactly(ch), pos);
  }

  constexpr size_type rfind(Chars sub, size_type pos = npos) const noexcept {
    return (sub.size_ > size_)
               ? npos
               : base::internal::ce_rfind(
                     sub.data_, sub.size_, data_,
                     base::internal::ce_min(pos, size_ - sub.size_));
  }

  template <typename Predicate, typename = predicate_t<Predicate>>
  constexpr bool contains(Predicate pred) const noexcept {
    return find(pred) != npos;
  }

  constexpr bool contains(char ch) const noexcept { return find(ch) != npos; }

  constexpr bool contains(Chars sub) const noexcept {
    return find(sub) != npos;
  }

  template <typename Predicate, typename = predicate_t<Predicate>>
  void ltrim(Predicate pred) {
    size_type i = 0;
    while (i < size_ && pred(data_[i])) ++i;
    remove_prefix(i);
  }

  void ltrim(char ch) { ltrim(base::charmatch::is_exactly(ch)); }
  void ltrim_whitespace() { ltrim(base::charmatch::is_whitespace()); }

  template <typename Predicate, typename = predicate_t<Predicate>>
  void rtrim(Predicate pred) {
    size_type i = size_;
    while (i > 0 && pred(data_[i - 1])) --i;
    remove_suffix(size_ - i);
  }

  void rtrim(char ch) { rtrim(base::charmatch::is_exactly(ch)); }
  void rtrim_whitespace() { rtrim(base::charmatch::is_whitespace()); }
  void rtrim_eol() { rtrim(base::charmatch::is_eol()); }

  template <typename Predicate, typename = predicate_t<Predicate>>
  void trim(Predicate pred) {
    ltrim(pred);
    rtrim(pred);
  }

  void trim(char ch) { trim(base::charmatch::is_exactly(ch)); }
  void trim_whitespace() { trim(base::charmatch::is_whitespace()); }

  std::vector<char> as_vector() const {
    return std::vector<char>(begin(), end());
  }

  void append_to(std::string* out) const {
    base::internal::append_chars(out, data(), size());
  }
  std::size_t length_hint() const noexcept { return size(); }
  std::string as_string() const { return std::string(data(), size()); }

  operator std::vector<char>() const { return as_vector(); }
  operator std::string() const { return as_string(); }
  operator re2::StringPiece() const noexcept {
    return re2::StringPiece(data(), size());
  }

  template <bool Dummy = true>
  constexpr operator If<Dummy && Mutable, Chars>() const noexcept {
    return Chars(data(), size());
  }

  constexpr operator BasicBytes<Mutable>() const noexcept {
    return BasicBytes<Mutable>(bytes(), size());
  }

  template <bool Dummy = true>
  constexpr operator If<Dummy && Mutable, Bytes>() const noexcept {
    return Bytes(bytes(), size());
  }

  std::size_t hash() const noexcept {
    return base::internal::hash_chars(data(), size());
  }

 private:
  pointer data_;
  size_type size_;
};

inline constexpr int compare(Chars a, Chars b) noexcept { return a.compare(b); }
inline constexpr bool operator==(Chars a, Chars b) noexcept {
  return a.compare(b) == 0;
}
inline constexpr bool operator!=(Chars a, Chars b) noexcept {
  return a.compare(b) != 0;
}
inline constexpr bool operator<(Chars a, Chars b) noexcept {
  return a.compare(b) < 0;
}
inline constexpr bool operator<=(Chars a, Chars b) noexcept {
  return a.compare(b) <= 0;
}
inline constexpr bool operator>(Chars a, Chars b) noexcept {
  return a.compare(b) > 0;
}
inline constexpr bool operator>=(Chars a, Chars b) noexcept {
  return a.compare(b) >= 0;
}

std::ostream& operator<<(std::ostream& o, Chars chars);

constexpr Chars substring(Chars sp, std::size_t pos,
                          std::size_t len = Chars::npos) noexcept {
  return sp.substring(pos, len);
}

constexpr Chars prefix(Chars sp, std::size_t len) noexcept {
  return sp.prefix(len);
}

constexpr Chars suffix(Chars sp, std::size_t len) noexcept {
  return sp.suffix(len);
}

constexpr bool has_prefix(Chars sp, Chars prefix) noexcept {
  return sp.has_prefix(prefix);
}

constexpr bool has_suffix(Chars sp, Chars suffix) noexcept {
  return sp.has_suffix(suffix);
}

constexpr Chars strip_prefix(Chars sp, std::size_t len) noexcept {
  return sp.strip_prefix(len);
}

constexpr Chars strip_suffix(Chars sp, std::size_t len) noexcept {
  return sp.strip_suffix(len);
}

constexpr Chars strip_prefix(Chars sp, Chars prefix) noexcept {
  return sp.strip_prefix(prefix);
}

constexpr Chars strip_suffix(Chars sp, Chars suffix) noexcept {
  return sp.strip_suffix(suffix);
}

namespace internal {

template <typename Predicate, typename T = decltype(std::declval<Predicate>()(
                                  std::declval<char>()))>
using is_predicate = std::is_convertible<T, bool>;

template <typename Predicate>
using predicate_t =
    typename std::enable_if<is_predicate<Predicate>::value, Predicate>::type;

}  // namespace internal

template <typename Predicate, typename = base::internal::predicate_t<Predicate>>
void ltrim(Predicate pred, std::string* str) {
  auto begin = str->begin(), end = str->end(), it = begin;
  while (it != end && pred(*it)) ++it;
  if (it != begin) str->erase(begin, it);
}
inline void ltrim(char ch, std::string* str) {
  ltrim(base::charmatch::is_exactly(ch), str);
}
inline void ltrim(const char* set, std::string* str) {
  ltrim(base::charmatch::is_oneof(set), str);
}
inline void ltrim_whitespace(std::string* str) {
  ltrim(base::charmatch::is_whitespace(), str);
}
inline void ltrim_eol(std::string* str) {
  ltrim(base::charmatch::is_eol(), str);
}

template <typename Predicate, typename = base::internal::predicate_t<Predicate>>
void rtrim(Predicate pred, std::string* str) {
  auto begin = str->begin(), end = str->end(), it = end;
  while (it != begin) {
    --it;
    if (!pred(*it)) {
      ++it;
      break;
    }
  }
  if (it != end) str->erase(it, end);
}
inline void rtrim(char ch, std::string* str) {
  rtrim(base::charmatch::is_exactly(ch), str);
}
inline void rtrim(const char* set, std::string* str) {
  rtrim(base::charmatch::is_oneof(set), str);
}
inline void rtrim_whitespace(std::string* str) {
  rtrim(base::charmatch::is_whitespace(), str);
}
inline void rtrim_eol(std::string* str) {
  rtrim(base::charmatch::is_eol(), str);
}

template <typename Predicate, typename = base::internal::predicate_t<Predicate>>
void trim(Predicate pred, std::string* str) {
  ltrim(pred, str);
  rtrim(pred, str);
}
inline void trim(char ch, std::string* str) {
  trim(base::charmatch::is_exactly(ch), str);
}
inline void trim(const char* set, std::string* str) {
  trim(base::charmatch::is_oneof(set), str);
}
inline void trim_whitespace(std::string* str) {
  trim(base::charmatch::is_whitespace(), str);
}
inline void trim_eol(std::string* str) { trim(base::charmatch::is_eol(), str); }

}  // namespace base

namespace std {
template <bool Mutable>
struct hash<base::BasicChars<Mutable>> {
  std::size_t operator()(base::BasicChars<Mutable> chars) const noexcept {
    return chars.hash();
  }
};
}  // namespace std

#endif  // BASE_CHARS_H
