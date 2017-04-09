// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_BYTES_H
#define BASE_BYTES_H

#include <array>
#include <climits>
#include <cstdint>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

namespace base {

namespace bytematch {

struct is_exactly {
  uint8_t value;

  constexpr is_exactly(uint8_t v) noexcept : value(v) {}
  constexpr bool operator()(uint8_t b) const noexcept { return b == value; }
};

struct is_oneof {
  const uint8_t* set;
  std::size_t len;

  constexpr is_oneof(const uint8_t* s, std::size_t l) noexcept : set(s),
                                                                 len(l) {}
  constexpr bool operator()(uint8_t b) const noexcept {
    return check(b, set, len);
  }

 private:
  static constexpr bool check(uint8_t b, const uint8_t* set,
                              std::size_t len) noexcept {
    return (len > 0) && ((b == *set) || check(b, set + 1, len - 1));
  }
};

}  // namespace bytematch

namespace internal {

std::size_t hash_bytes(const uint8_t* ptr, std::size_t len) noexcept;

template <typename T>
inline constexpr T ce_min(T a, T b) noexcept {
  return (a < b) ? a : b;
}

template <typename Unit>
inline constexpr int ce_memcmp(const Unit* p, const Unit* q,
                               std::size_t n) noexcept {
  return (n == 0)
             ? 0
             : ((*p < *q) ? -1
                          : ((*p > *q) ? 1 : ce_memcmp(p + 1, q + 1, n - 1)));
}

template <typename Unit>
inline constexpr bool ce_memeq(const Unit* p, const Unit* q,
                               std::size_t n) noexcept {
  return ce_memcmp(p, q, n) == 0;
}

inline constexpr int ce_compare_final(std::size_t a, std::size_t b,
                                      int n) noexcept {
  return (n != 0) ? n : ((a < b) ? -1 : ((a > b) ? 1 : 0));
}

template <typename Unit>
inline constexpr int ce_compare(const Unit* aptr, const Unit* bptr,
                                std::size_t alen, std::size_t blen) noexcept {
  return ce_compare_final(alen, blen,
                          ce_memcmp(aptr, bptr, ce_min(alen, blen)));
}

template <typename Unit, typename Predicate>
inline constexpr std::size_t ce_find(Predicate pred, const Unit* ptr,
                                     std::size_t len,
                                     std::size_t index) noexcept {
  return (index >= len)
             ? SIZE_MAX
             : (pred(ptr[index]) ? index : ce_find(pred, ptr, len, index + 1));
}

template <typename Unit>
inline constexpr std::size_t ce_find(const Unit* sptr, std::size_t slen,
                                     const Unit* ptr, std::size_t len,
                                     std::size_t index) noexcept {
  return (slen > len || index > len - slen)
             ? SIZE_MAX
             : (ce_memeq(ptr + index, sptr, slen)
                    ? index
                    : ce_find(sptr, slen, ptr, len, index + 1));
}

template <typename Unit, typename Predicate>
inline constexpr std::size_t ce_rfind(Predicate pred, const Unit* ptr,
                                      std::size_t index) noexcept {
  return pred(ptr[index])
             ? index
             : ((index == 0) ? SIZE_MAX : ce_rfind(pred, ptr, index - 1));
}

template <typename Unit>
inline constexpr std::size_t ce_rfind(const Unit* sptr, std::size_t slen,
                                      const Unit* ptr, std::size_t index) {
  return ce_memeq(ptr + index, sptr, slen)
             ? index
             : ((index == 0) ? SIZE_MAX : ce_rfind(sptr, slen, ptr, index - 1));
}

}  // namespace internal

template <bool Mutable>
class BasicBytes;

// Represents a view into an immutable byte buffer.
//
// NOTE: Bytes does not own the memory it points to.
//       Use std::vector<uint8_t> or alternatives if you need memory to persist.
//       In particular, Bytes is rarely appropriate as an object member.
//
using Bytes = BasicBytes<false>;

// Represents a view into a mutable byte buffer.
//
// NOTE: MutableBytes does not own the memory it points to.
//       Use std::vector<uint8_t> or alternatives if you need memory to persist.
//       In particular, MutableBytes is rarely appropriate as an object member.
//
using MutableBytes = BasicBytes<true>;

template <bool Mutable>
class BasicBytes {
 private:
  template <bool B, typename T = void>
  using If = typename std::enable_if<B, T>::type;

  template <bool B, typename T, typename U>
  using Cond = typename std::conditional<B, T, U>::type;

  template <typename Predicate, typename T = decltype(std::declval<Predicate>()(
                                    std::declval<uint8_t>()))>
  using is_predicate = std::is_convertible<T, bool>;

  template <typename Predicate>
  using predicate_t = If<is_predicate<Predicate>::value, Predicate>;

  template <typename Container, typename T = decltype(std::declval<Container&>().data())>
  using is_container = std::is_convertible<T, const uint8_t*>;

  template <typename Container>
  using container_t = If<is_container<Container>::value, Container>;

  template <typename Container, typename T = decltype(std::declval<Container&>().data())>
  using is_char_container = std::is_same<T, const char*>;

  template <typename Container>
  using char_container_t = If<is_char_container<Container>::value, Container>;

 public:
  using value_type = uint8_t;
  using const_reference = const uint8_t&;
  using const_pointer = const uint8_t*;
  using mutable_reference = uint8_t&;
  using mutable_pointer = uint8_t*;
  using reference = Cond<Mutable, mutable_reference, const_reference>;
  using pointer = Cond<Mutable, mutable_pointer, const_pointer>;

  using const_iterator = const_pointer;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using iterator = pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;

  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  template <size_type N>
  using carray_reference = Cond<Mutable, uint8_t (&)[N], const uint8_t (&)[N]>;

  template <size_type N>
  using array_reference =
      Cond<Mutable, std::array<uint8_t, N>&, const std::array<uint8_t, N>&>;

  using vector_reference =
      Cond<Mutable, std::vector<uint8_t>&, const std::vector<uint8_t>&>;

  using char_pointer = Cond<Mutable, char*, const char*>;

  template <size_type N>
  using char_carray_reference = Cond<Mutable, char (&)[N], const char (&)[N]>;

  template <size_type N>
  using char_array_reference =
      Cond<Mutable, std::array<char, N>&, const std::array<char, N>&>;

  using char_vector_reference =
      Cond<Mutable, std::vector<char>&, const std::vector<char>&>;

  static constexpr size_type npos = SIZE_MAX;

  constexpr BasicBytes(std::nullptr_t = nullptr, size_type = 0) noexcept
      : data_(nullptr),
        size_(0) {}

  constexpr BasicBytes(pointer ptr, size_type len) noexcept : data_(ptr),
                                                              size_(len) {}

  template <size_type N>
  constexpr BasicBytes(carray_reference<N> a) noexcept : data_(a), size_(N) {}

  template <size_type N>
  constexpr BasicBytes(array_reference<N> a) noexcept : data_(a.data()),
                                                        size_(N) {}

  BasicBytes(vector_reference v) : data_(v.data()), size_(v.size()) {}

  constexpr BasicBytes(char_pointer ptr, size_type len) noexcept
      : data_(reinterpret_cast<pointer>(ptr)),
        size_(len) {}

  template <size_type N>
  constexpr BasicBytes(char_carray_reference<N> a) noexcept
      : data_(reinterpret_cast<pointer>(a)),
        size_(N) {}

  template <size_type N>
  constexpr BasicBytes(char_array_reference<N> a) noexcept
      : data_(reinterpret_cast<pointer>(a.data())),
        size_(N) {}

  BasicBytes(char_vector_reference v) noexcept : data_(v.data()),
                                                          size_(v.size()) {}

  template <bool Dummy = true>
  BasicBytes(If<Dummy && !Mutable, const std::string&> s) noexcept
      : BasicBytes(s.data(), s.size()) {}

  constexpr BasicBytes(const BasicBytes&) noexcept = default;
  constexpr BasicBytes(BasicBytes&&) noexcept = default;

  BasicBytes& operator=(const BasicBytes&) noexcept = default;
  BasicBytes& operator=(BasicBytes&&) noexcept = default;

  template <typename Container, typename = container_t<Container>>
  BasicBytes& operator=(const Container& c) noexcept {
    data_ = c.data();
    size_ = c.size();
    return *this;
  }

  template <typename Container, typename = void, typename = char_container_t<Container>>
  BasicBytes& operator=(const Container& c) noexcept {
    data_ = reinterpret_cast<pointer>(c.data());
    size_ = c.size();
    return *this;
  }

  template <typename... Args>
  void assign(Args&&... args) noexcept {
    *this = BasicBytes(std::forward<Args>(args)...);
  }

  constexpr bool empty() const noexcept { return size_ == 0; }
  constexpr pointer data() const noexcept { return data_; }
  constexpr size_type size() const noexcept { return size_; }

  constexpr char_pointer chars() const noexcept {
    return reinterpret_cast<char_pointer>(data());
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

  constexpr BasicBytes substring(size_type pos, size_type len = npos) {
    return (pos >= size_)
               ? BasicBytes(data_ + size_, 0)
               : ((len > size_ - pos) ? BasicBytes(data_ + pos, size_ - pos)
                                      : BasicBytes(data_ + pos, len));
  }
  constexpr BasicBytes substr(size_type pos, size_type len = npos) {
    return substring(pos, len);
  }

  constexpr BasicBytes prefix(size_type n) const noexcept {
    return (size_ >= n) ? BasicBytes(data_, n) : *this;
  }
  constexpr BasicBytes suffix(size_type n) const noexcept {
    return (size_ >= n) ? BasicBytes(data_ + size_ - n, n) : *this;
  }

  constexpr bool has_prefix(Bytes pre) const noexcept {
    return size_ >= pre.size_ &&
           base::internal::ce_memeq(data_, pre.data_, pre.size_);
  }
  constexpr bool has_suffix(Bytes suf) const noexcept {
    return size_ >= suf.size_ &&
           base::internal::ce_memeq(data_ + size_ - suf.size_, suf.data_,
                                    suf.size_);
  }

  constexpr BasicBytes strip_prefix(size_type len) const noexcept {
    return substring(len);
  }
  constexpr BasicBytes strip_suffix(size_type len) const noexcept {
    return substring(0, (size_ >= len) ? (size_ - len) : 0);
  }

  constexpr BasicBytes strip_prefix(Bytes pre) const noexcept {
    return has_prefix(pre) ? substring(pre.size()) : *this;
  }
  constexpr BasicBytes strip_suffix(Bytes suf) const noexcept {
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

  bool remove_prefix(Bytes pre) noexcept {
    if (!has_prefix(pre)) return false;
    remove_prefix(pre.size());
    return true;
  }
  bool remove_suffix(Bytes suf) noexcept {
    if (!has_suffix(suf)) return false;
    remove_suffix(suf.size());
    return true;
  }

  template <typename Predicate, typename = predicate_t<Predicate>>
  constexpr size_type find(Predicate pred, size_type pos = 0) const noexcept {
    return base::internal::ce_find(pred, data_, size_, pos);
  }

  constexpr size_type find(uint8_t b, size_type pos = 0) const noexcept {
    return find(base::bytematch::is_exactly(b), pos);
  }

  constexpr size_type find(Bytes sub, size_type pos = 0) const noexcept {
    return base::internal::ce_find(sub.data_, sub.size_, data_, size_, pos);
  }

  template <typename Predicate, typename = predicate_t<Predicate>>
  constexpr size_type rfind(Predicate pred, size_type pos = npos) const
      noexcept {
    return empty() ? npos
                   : base::internal::ce_rfind(
                         pred, data_, base::internal::ce_min(pos, size_ - 1));
  }

  constexpr size_type rfind(uint8_t b, size_type pos = npos) const noexcept {
    return rfind(base::bytematch::is_exactly(b), pos);
  }

  constexpr size_type rfind(Bytes sub, size_type pos = npos) const noexcept {
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

  constexpr bool contains(uint8_t b) const noexcept { return find(b) != npos; }

  constexpr bool contains(Bytes sub) const noexcept {
    return find(sub) != npos;
  }

  template <typename Predicate, typename = predicate_t<Predicate>>
  void ltrim(Predicate pred) {
    size_type i = 0;
    while (i < size_ && pred(data_[i])) ++i;
    remove_prefix(i);
  }

  void ltrim(uint8_t b) { ltrim(base::bytematch::is_exactly(b)); }

  template <typename Predicate, typename = predicate_t<Predicate>>
  void rtrim(Predicate pred) {
    size_type i = size_;
    while (i > 0 && pred(data_[i - 1])) --i;
    remove_suffix(size_ - i);
  }

  void rtrim(uint8_t b) { rtrim(base::bytematch::is_exactly(b)); }

  template <typename Predicate, typename = predicate_t<Predicate>>
  void trim(Predicate pred) {
    ltrim(pred);
    rtrim(pred);
  }

  void trim(uint8_t b) { trim(base::bytematch::is_exactly(b)); }

  std::string as_string() const { return std::string(chars(), size()); }

  std::vector<uint8_t> as_vector() const {
    return std::vector<uint8_t>(begin(), end());
  }

  operator std::vector<uint8_t>() const { return as_vector(); }

  template <bool Dummy = true>
  constexpr operator If<Dummy && Mutable, Bytes>() const noexcept {
    return Bytes(data(), size());
  }

  std::size_t hash() const noexcept {
    return base::internal::hash_bytes(data(), size());
  }

 private:
  pointer data_;
  size_type size_;
};

inline constexpr int compare(Bytes a, Bytes b) noexcept { return a.compare(b); }
inline constexpr bool operator==(Bytes a, Bytes b) noexcept {
  return a.compare(b) == 0;
}
inline constexpr bool operator!=(Bytes a, Bytes b) noexcept {
  return a.compare(b) != 0;
}
inline constexpr bool operator<(Bytes a, Bytes b) noexcept {
  return a.compare(b) < 0;
}
inline constexpr bool operator<=(Bytes a, Bytes b) noexcept {
  return a.compare(b) <= 0;
}
inline constexpr bool operator>(Bytes a, Bytes b) noexcept {
  return a.compare(b) > 0;
}
inline constexpr bool operator>=(Bytes a, Bytes b) noexcept {
  return a.compare(b) >= 0;
}

}  // namespace base

namespace std {
template <bool Mutable>
struct hash<base::BasicBytes<Mutable>> {
  std::size_t operator()(base::BasicBytes<Mutable> bytes) const noexcept {
    return bytes.hash();
  }
};
}  // namespace std

#endif  // BASE_BYTES_H
