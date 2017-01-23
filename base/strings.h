// base/strings.h - StringPiece and other string helpers
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_STRINGS_H
#define BASE_STRINGS_H

#include <climits>
#include <cstring>
#include <functional>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/concat.h"
#include "external/com_googlesource_code_re2/re2/stringpiece.h"

namespace base {

struct is_exactly {
  char c;

  constexpr is_exactly(char ch) noexcept : c(ch) {}
  constexpr bool operator()(char ch) const noexcept { return ch == c; }
};

struct is_oneof {
  const char* set;

  constexpr is_oneof(const char* s) noexcept : set(s) {}
  constexpr bool operator()(char ch) const noexcept { return check(ch, set); }

 private:
  static constexpr bool check(char ch, const char* set) noexcept {
    return *set && ((ch == *set) || check(ch, set + 1));
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

  static constexpr size_type npos = SIZE_MAX;

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

  static constexpr bool ce_memeq(const_pointer p, const_pointer q,
                                 size_type n) noexcept {
    return ce_memcmp(p, q, n) == 0;
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

  static constexpr size_type ce_find(char ch, const_pointer ptr, size_type len,
                                     size_type index) noexcept {
    return (index >= len) ? npos : ((ptr[index] == ch)
                                        ? index
                                        : ce_find(ch, ptr, len, index + 1));
  }

  static constexpr size_type ce_find(const_pointer sptr, size_type slen,
                                     const_pointer ptr, size_type len,
                                     size_type index) noexcept {
    return (slen > len || index > len - slen)
               ? npos
               : (ce_memeq(ptr + index, sptr, slen)
                      ? index
                      : ce_find(sptr, slen, ptr, len, index + 1));
  }

  static constexpr size_type ce_rfind(char ch, const_pointer ptr,
                                      size_type index) noexcept {
    return (ptr[index] == ch)
               ? index
               : ((index == 0) ? npos : ce_rfind(ch, ptr, index - 1));
  }

  static constexpr size_type ce_rfind(const_pointer sptr, size_type slen,
                                      const_pointer ptr, size_type index) {
    return ce_memeq(ptr + index, sptr, slen)
               ? index
               : ((index == 0) ? npos : ce_rfind(sptr, slen, ptr, index - 1));
  }

  static constexpr size_type cap(size_type pos, size_type sz) noexcept {
    return (pos > sz) ? sz : pos;
  }

 public:
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

  constexpr StringPiece strip_prefix(size_type len) const noexcept {
    return substring(len);
  }

  constexpr StringPiece strip_prefix(StringPiece sp) const noexcept {
    return has_prefix(sp) ? substring(sp.size()) : *this;
  }

  constexpr StringPiece strip_suffix(size_type len) const noexcept {
    return substring(0, (size_ >= len) ? (size_ - len) : 0);
  }

  constexpr StringPiece strip_suffix(StringPiece sp) const noexcept {
    return has_suffix(sp) ? substring(0, size_ - sp.size_) : *this;
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

  template <typename Predicate>
  void ltrim(Predicate pred) {
    while (!empty() && pred(front())) remove_prefix(1);
  }
  void ltrim(char ch) { ltrim(is_exactly(ch)); }
  void ltrim_whitespace() { ltrim(is_whitespace()); }

  template <typename Predicate>
  void rtrim(Predicate pred) {
    while (!empty() && pred(back())) remove_suffix(1);
  }
  void rtrim(char ch) { rtrim(is_exactly(ch)); }
  void rtrim_whitespace() { rtrim(is_whitespace()); }

  template <typename Predicate>
  void trim(Predicate pred) {
    ltrim(pred);
    rtrim(pred);
  }
  void trim(char ch) { trim(is_exactly(ch)); }
  void trim_whitespace() { trim(is_whitespace()); }

  constexpr bool contains(StringPiece sp) const noexcept {
    return find(sp) != npos;
  }

  constexpr size_type find(char ch, size_type pos = 0) const noexcept {
    return ce_find(ch, data_, size_, pos);
  }

  constexpr size_type find(StringPiece sp, size_type pos = 0) const noexcept {
    return ce_find(sp.data_, sp.size_, data_, size_, pos);
  }

  constexpr size_type rfind(char ch, size_type pos = npos) const noexcept {
    return empty() ? npos : ce_rfind(ch, data_, cap(pos, size_ - 1));
  }

  constexpr size_type rfind(StringPiece sp, size_type pos = npos) const
      noexcept {
    return (sp.size_ > size_) ? npos : ce_rfind(sp.data_, sp.size_, data_,
                                                cap(pos, size_ - sp.size_));
  }

  std::size_t hash() const noexcept;

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

constexpr StringPiece strip_prefix(StringPiece sp,
                                   StringPiece::size_type len) noexcept {
  return sp.strip_prefix(len);
}

constexpr StringPiece strip_prefix(StringPiece sp,
                                   StringPiece prefix) noexcept {
  return sp.strip_prefix(prefix);
}

constexpr StringPiece strip_suffix(StringPiece sp,
                                   StringPiece::size_type len) noexcept {
  return sp.strip_suffix(len);
}

constexpr StringPiece strip_suffix(StringPiece sp,
                                   StringPiece suffix) noexcept {
  return sp.strip_suffix(suffix);
}

template <typename Predicate>
void ltrim(Predicate pred, std::string* str) {
  auto begin = str->begin(), end = str->end(), it = begin;
  while (it != end && pred(*it)) ++it;
  if (it != begin) str->erase(begin, it);
}
inline void ltrim(char ch, std::string* str) { ltrim(is_exactly(ch), str); }
inline void ltrim(const char* set, std::string* str) {
  ltrim(is_oneof(set), str);
}
inline void ltrim_whitespace(std::string* str) { ltrim(is_whitespace(), str); }
inline void ltrim_eol(std::string* str) { ltrim(is_eol(), str); }

template <typename Predicate>
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
inline void rtrim(char ch, std::string* str) { rtrim(is_exactly(ch), str); }
inline void rtrim(const char* set, std::string* str) {
  rtrim(is_oneof(set), str);
}
inline void rtrim_whitespace(std::string* str) { rtrim(is_whitespace(), str); }
inline void rtrim_eol(std::string* str) { rtrim(is_eol(), str); }

template <typename Predicate>
void trim(Predicate pred, std::string* str) {
  ltrim(pred, str);
  rtrim(pred, str);
}
inline void trim(char ch, std::string* str) { trim(is_exactly(ch), str); }
inline void trim(const char* set, std::string* str) {
  trim(is_oneof(set), str);
}
inline void trim_whitespace(std::string* str) { trim(is_whitespace(), str); }
inline void trim_eol(std::string* str) { trim(is_eol(), str); }

class SplitterImpl {
 protected:
  SplitterImpl() noexcept = default;

 public:
  virtual ~SplitterImpl() noexcept = default;

  // If |sp| can be split, splits it into |first| + |rest| and returns true.
  // Otherwise, copies |sp| to |first| and returns false.
  //
  // Example (splitting on ','):
  //    sp      | return  | first | rest
  //    --------+---------+-------+----------
  //    "a,b,c" | true    | "a"   | "b,c"
  //    "b,c"   | true    | "b"   | "c"
  //    "c"     | false   | "c"   | <ignored>
  virtual bool chop(StringPiece* first, StringPiece* rest,
                    StringPiece sp) const = 0;

  SplitterImpl(const SplitterImpl&) = delete;
  SplitterImpl(SplitterImpl&&) = delete;
  SplitterImpl& operator=(const SplitterImpl&) = delete;
  SplitterImpl& operator=(SplitterImpl&&) = delete;
};

class Splitter {
 public:
  using Pointer = std::shared_ptr<SplitterImpl>;
  using Predicate = std::function<bool(char)>;

  // Splitter is constructible from an implementation.
  Splitter(Pointer ptr) : ptr_(std::move(ptr)), lim_(SIZE_MAX), omit_(false) {}

  // Splitter is default constructible.
  Splitter() : Splitter(nullptr) {}

  // Splitter is copyable and moveable.
  Splitter(const Splitter&) = default;
  Splitter(Splitter&&) = default;
  Splitter& operator=(const Splitter&) = default;
  Splitter& operator=(Splitter&&) = default;

  // Returns true iff this Splitter is non-empty.
  explicit operator bool() const noexcept { return !!ptr_; }

  // Asserts that this Splitter is non-empty.
  void assert_valid() const noexcept;

  // Returns this Splitter's implementation.
  const Pointer& implementation() const noexcept { return ptr_; }
  Pointer& implementation() noexcept { return ptr_; }

  // Trims all characters matching |pred|
  // from the beginning and end of each item.
  Splitter& trim(Predicate pred) {
    trim_ = std::move(pred);
    return *this;
  }

  // Trims |ch| from the beginning and end of each item.
  Splitter& trim(char ch) {
    trim_ = is_exactly(ch);
    return *this;
  }

  // Trims whitespace from the beginning and end of each item.
  Splitter& trim_whitespace() {
    trim_ = is_whitespace();
    return *this;
  }

  // Limits the output to |n| items.
  // - 0 is impossible; it is treated as 1
  Splitter& limit(std::size_t n) noexcept {
    lim_ = n;
    return *this;
  }

  // Removes any limit on the number of items to be output.
  Splitter& unlimited() noexcept {
    lim_ = SIZE_MAX;
    return *this;
  }

  // Make this Splitter omit empty items.
  //
  // Example (splitting on ','):
  //                Input: "a,,b,c"
  //    Output (standard): {"a", "", "b", "c"}
  //  Output (omit_empty): {"a", "b", "c"}
  //
  Splitter& omit_empty(bool value = true) noexcept {
    omit_ = value;
    return *this;
  }

  // Splits |sp| into pieces.
  std::vector<StringPiece> split(StringPiece sp) const;

  // Splits |sp| into pieces.  The pieces are returned as std::strings.
  std::vector<std::string> split_strings(StringPiece sp) const;

 private:
  Pointer ptr_;
  Predicate trim_;
  std::size_t lim_;
  bool omit_;
};

class JoinerImpl {
 protected:
  JoinerImpl() noexcept = default;

 public:
  virtual ~JoinerImpl() noexcept = default;

  virtual void glue(std::string* out, StringPiece sp, bool first) const = 0;
  virtual std::size_t hint() const noexcept = 0;

  JoinerImpl(const JoinerImpl&) = delete;
  JoinerImpl(JoinerImpl&&) = delete;
  JoinerImpl& operator=(const JoinerImpl&) = delete;
  JoinerImpl& operator=(JoinerImpl&&) = delete;
};

class Join {
 public:
  using Pointer = std::shared_ptr<JoinerImpl>;
  using Vector = std::vector<std::string>;

  Join(const Join&) = default;
  Join(Join&&) noexcept = default;
  Join& operator=(const Join&) = default;
  Join& operator=(Join&&) noexcept = default;

  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept;

  operator std::string() const {
    std::string out;
    append_to(&out);
    return out;
  }

 private:
  friend class Joiner;

  Join(Vector v, Pointer p, bool s) noexcept : vec_(std::move(v)),
                                               ptr_(std::move(p)),
                                               skip_(s) {}

  Vector vec_;
  Pointer ptr_;
  bool skip_;
};

class Joiner {
 public:
  using Pointer = std::shared_ptr<JoinerImpl>;

  // Joiner is constructible from an implementation.
  Joiner(Pointer ptr) : ptr_(std::move(ptr)), skip_(false) {}

  // Joiner is default constructible.
  Joiner() : Joiner(nullptr) {}

  // Joiner is copyable and moveable.
  Joiner(const Joiner&) = default;
  Joiner(Joiner&&) = default;
  Joiner& operator=(const Joiner&) = default;
  Joiner& operator=(Joiner&&) = default;

  // Returns true iff this Joiner is non-empty.
  explicit operator bool() const noexcept { return !!ptr_; }

  // Asserts that this Joiner is non-empty.
  void assert_valid() const noexcept;

  // Returns this Joiner's implementation.
  const Pointer& implementation() const noexcept { return ptr_; }
  Pointer& implementation() noexcept { return ptr_; }

  // Make this Joiner skip empty items.
  //
  // Example (joining on ','):
  //                Input: {"a", "", "b", "c"}
  //    Output (standard): "a,,b,c"
  //  Output (omit_empty): "a,b,c"
  //
  Joiner& skip_empty(bool value = true) noexcept {
    skip_ = value;
    return *this;
  }

 private:
  static void build_vector(std::vector<std::string>* out) {}

  template <typename T>
  static void build_vector(std::vector<std::string>* out, T&& arg) {
    std::string tmp;
    using base::append_to;
    append_to(&tmp, std::forward<T>(arg));
    out->push_back(std::move(tmp));
  }

  template <typename First, typename Second, typename... Rest>
  static void build_vector(std::vector<std::string>* out, First&& first,
                           Second&& second, Rest&&... rest) {
    build_vector(out, std::forward<First>(first));
    build_vector(out, std::forward<Second>(second),
                 std::forward<Rest>(rest)...);
  }

 public:
  Join join(const std::vector<StringPiece>& vec) const;
  Join join(const std::vector<std::string>& vec) const;

  template <typename... Args>
  Join join(Args&&... args) const {
    assert_valid();
    std::vector<StringPiece> vec;
    vec.resize(sizeof...(args));
    build_vector(&vec, std::forward<Args>(args)...);
    return Join(vec, ptr_, skip_);
  }

  void join_append(std::string* out,
                   const std::vector<StringPiece>& vec) const {
    join(vec).append_to(out);
  }
  void join_append(std::string* out,
                   const std::vector<std::string>& vec) const {
    join(vec).append_to(out);
  }
  template <typename... Args>
  void join_append(std::string* out, Args&&... args) const {
    join(std::forward<Args>(args)...).append_to(out);
  }

  std::string join_string(const std::vector<StringPiece>& vec) const {
    return join(vec);
  }
  std::string join_string(const std::vector<std::string>& vec) const {
    return join(vec);
  }
  template <typename... Args>
  std::string join_string(Args&&... args) const {
    return join(std::forward<Args>(args)...);
  }

 private:
  Pointer ptr_;
  bool skip_;
};

namespace split {

using Predicate = std::function<bool(char)>;

Splitter fixed_length(std::size_t len);
Splitter on(char ch);
Splitter on(std::string str);
Splitter on(Predicate pred);
Splitter on_pattern(StringPiece pattern);

}  // namespace split
namespace join {

Joiner on();
Joiner on(char ch);
Joiner on(std::string str);

}  // namespace join
}  // namespace base

namespace std {
template <>
struct hash<base::StringPiece> {
  std::size_t operator()(base::StringPiece sp) const noexcept { return sp.hash(); }
};
}  // namespace std

#endif  // BASE_STRINGS_H
