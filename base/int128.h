#ifndef BASE_INT128_H
#define BASE_INT128_H

#include <cstdint>
#include <iosfwd>
#include <limits>
#include <stdexcept>

namespace base {

class UInt128 {
 private:
  static constexpr uint64_t U64MAX = 0xffffffffffffffffULL;

 public:
  static_assert(U64MAX == uint64_t(~0ULL),
                "this class assumes that uint64_t is exactly 64 bits");

  constexpr static UInt128 min() noexcept { return UInt128(0U, 0U); }
  constexpr static UInt128 max() noexcept { return UInt128(U64MAX, U64MAX); }

  constexpr UInt128() noexcept : lo_(0), hi_(0) {}
  constexpr UInt128(uint64_t x) noexcept : lo_(x), hi_(0) {}
  constexpr explicit UInt128(uint64_t hi, uint64_t lo) noexcept : lo_(lo),
                                                                  hi_(hi) {}
  constexpr UInt128(const UInt128&) noexcept = default;
  constexpr UInt128(UInt128&&) noexcept = default;
  UInt128& operator=(const UInt128&) noexcept = default;
  UInt128& operator=(UInt128&&) noexcept = default;
  UInt128& operator=(uint64_t x) noexcept {
    lo_ = x;
    hi_ = 0;
    return *this;
  }

  constexpr explicit operator bool() const noexcept { return lo_ || hi_; }

  constexpr bool is_zero() const noexcept { return lo_ == 0 && hi_ == 0; }

  constexpr bool bit(unsigned int n) const noexcept {
    return (n >= 128) ? false : ((n >= 64) ? (hi_ & (1ULL << (n - 64)))
                                           : (lo_ & (1ULL << n)));
  }

  friend constexpr bool operator==(UInt128 a, UInt128 b) noexcept {
    return a.lo_ == b.lo_ && a.hi_ == b.hi_;
  }
  friend constexpr bool operator!=(UInt128 a, UInt128 b) noexcept {
    return !(a == b);
  }
  friend constexpr bool operator<(UInt128 a, UInt128 b) noexcept {
    return a.hi_ < b.hi_ || (a.hi_ == b.hi_ && a.lo_ < b.lo_);
  }
  friend constexpr bool operator>(UInt128 a, UInt128 b) noexcept {
    return (b < a);
  }
  friend constexpr bool operator<=(UInt128 a, UInt128 b) noexcept {
    return !(b < a);
  }
  friend constexpr bool operator>=(UInt128 a, UInt128 b) noexcept {
    return !(a < b);
  }

  friend constexpr UInt128 operator&(UInt128 a, UInt128 b) noexcept {
    return UInt128(a.hi_ & b.hi_, a.lo_ & b.lo_);
  }
  friend constexpr UInt128 operator|(UInt128 a, UInt128 b) noexcept {
    return UInt128(a.hi_ | b.hi_, a.lo_ | b.lo_);
  }
  friend constexpr UInt128 operator^(UInt128 a, UInt128 b) noexcept {
    return UInt128(a.hi_ ^ b.hi_, a.lo_ ^ b.lo_);
  }

  UInt128& operator&=(UInt128 b) noexcept { return (*this = (*this & b)); }
  UInt128& operator|=(UInt128 b) noexcept { return (*this = (*this | b)); }
  UInt128& operator^=(UInt128 b) noexcept { return (*this = (*this ^ b)); }

  friend constexpr UInt128 operator<<(UInt128 a, unsigned int n) noexcept {
    return (n >= 128)
               ? UInt128()
               : ((n >= 64)
                      ? UInt128(a.lo_ << (n - 64), 0U)
                      : ((n > 0) ? UInt128((a.hi_ << n) | (a.lo_ >> (64 - n)),
                                           (a.lo_ << n))
                                 : a));
  }
  friend constexpr UInt128 operator>>(UInt128 a, unsigned int n) noexcept {
    return (n >= 128)
               ? UInt128()
               : ((n >= 64)
                      ? UInt128(0U, a.hi_ >> (n - 64))
                      : ((n > 0) ? UInt128((a.hi_ >> n),
                                           (a.lo_ >> n) | (a.hi_ << (64 - n)))
                                 : a));
  }

  UInt128& operator<<=(unsigned int n) noexcept {
    return (*this = (*this << n));
  }
  UInt128& operator>>=(unsigned int n) noexcept {
    return (*this = (*this >> n));
  }

  constexpr UInt128 operator+() const noexcept { return *this; }
  constexpr UInt128 operator~() const noexcept { return UInt128(~hi_, ~lo_); }
  constexpr UInt128 operator-() const noexcept { return ~*this + 1U; }

  friend constexpr UInt128 operator+(UInt128 a, UInt128 b) noexcept {
    return UInt128(a.hi_ + b.hi_ + carry(a.lo_, b.lo_), a.lo_ + b.lo_);
  }
  friend constexpr UInt128 operator-(UInt128 a, UInt128 b) noexcept {
    return a + (-b);
  }
  friend UInt128 operator*(UInt128 a, UInt128 b) noexcept;
  friend std::pair<UInt128, UInt128> divmod(UInt128 a, UInt128 b);
  friend UInt128 operator/(UInt128 a, UInt128 b) { return divmod(a, b).first; }
  friend UInt128 operator%(UInt128 a, UInt128 b) { return divmod(a, b).second; }

  UInt128& operator+=(UInt128 b) noexcept { return (*this = (*this + b)); }
  UInt128& operator-=(UInt128 b) noexcept { return (*this = (*this - b)); }
  UInt128& operator*=(UInt128 b) noexcept { return (*this = (*this * b)); }
  UInt128& operator/=(UInt128 b) noexcept { return (*this = (*this / b)); }
  UInt128& operator%=(UInt128 b) noexcept { return (*this = (*this % b)); }

  UInt128& operator++() noexcept { return (*this += 1); }
  UInt128& operator--() noexcept { return (*this -= 1); }

  UInt128 operator++(int)noexcept {
    UInt128 old(*this);
    ++*this;
    return old;
  }
  UInt128 operator--(int)noexcept {
    UInt128 old(*this);
    --*this;
    return old;
  }

  template <typename T, typename = typename std::enable_if<
                            std::is_integral<T>::value>::type>
  constexpr explicit operator T() const {
    using U = typename std::make_unsigned<T>::type;
    return (hi_ != 0 || lo_ > U(std::numeric_limits<T>::max()))
               ? (throw std::overflow_error("result out of range"), 0)
               : U(lo_);
  }

  std::string as_string(unsigned int radix = 10) const;

 private:
  friend class Int128;

  constexpr static uint64_t carry(uint64_t a, uint64_t b) {
    return ((a + b) < a) ? 1 : 0;
  }

  uint64_t lo_;
  uint64_t hi_;
};

std::ostream& operator<<(std::ostream& o, UInt128 x);

}  // namespace base

#endif  // BASE_INT128_H
