#ifndef BASE_SAFEMATH_H
#define BASE_SAFEMATH_H

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace base {

template <typename T>
class safe {
 public:
  using value_type = T;
  static_assert(std::is_integral<T>::value, "");

  constexpr safe(T x = 0) noexcept : value_(x) {}
  constexpr safe(const safe&) noexcept = default;
  constexpr safe(safe&&) noexcept = default;
  template <typename U>
  constexpr safe(safe<U> x) : value_(convert<T, U>(x.value())) {}

  safe& operator=(const safe&) noexcept = default;
  safe& operator=(safe&&) noexcept = default;
  safe& operator=(T x) noexcept {
    value_ = x;
    return *this;
  }
  template <typename U>
  safe& operator=(safe<U> x) {
    value_ = convert<T, U>(x.value());
    return *this;
  }

  constexpr explicit operator bool() const noexcept { return value_; }

  constexpr T value() const noexcept { return value_; }

  template <typename U>
  constexpr U value() const noexcept { return safe<U>(*this).value(); }

  friend constexpr bool operator==(safe a, safe b) noexcept {
    return a.value_ == b.value_;
  }
  friend constexpr bool operator!=(safe a, safe b) noexcept {
    return a.value_ != b.value_;
  }
  friend constexpr bool operator<(safe a, safe b) noexcept {
    return a.value_ < b.value_;
  }
  friend constexpr bool operator>(safe a, safe b) noexcept {
    return a.value_ > b.value_;
  }
  friend constexpr bool operator<=(safe a, safe b) noexcept {
    return a.value_ <= b.value_;
  }
  friend constexpr bool operator>=(safe a, safe b) noexcept {
    return a.value_ >= b.value_;
  }

  constexpr safe operator+() const noexcept { return *this; }
  constexpr safe operator-() const { return safe(negate(value_)); }
  constexpr safe abs() const { return safe(abs(value_)); }
  constexpr safe sgn() const noexcept { return safe(sgn(value_)); }

  friend constexpr std::pair<safe, safe> divmod(safe a, safe b) {
    return divmod_impl(a, b);
  }

  friend constexpr safe operator+(safe a, safe b) { return add_impl(a, b); }
  friend constexpr safe operator-(safe a, safe b) { return sub_impl(a, b); }
  friend constexpr safe operator*(safe a, safe b) { return mul_impl(a, b); }
  friend constexpr safe operator/(safe a, safe b) { return divmod(a, b).first; }
  friend constexpr safe operator%(safe a, safe b) {
    return divmod(a, b).second;
  }

  safe& operator+=(safe b) { return (*this = (*this + b)); }
  safe& operator-=(safe b) { return (*this = (*this - b)); }
  safe& operator*=(safe b) { return (*this = (*this * b)); }
  safe& operator/=(safe b) { return (*this = (*this / b)); }
  safe& operator%=(safe b) { return (*this = (*this % b)); }

  safe& operator++() { return (*this += 1); }
  safe& operator--() { return (*this -= 1); }

  safe operator++(int) {
    auto old = *this;
    ++*this;
    return old;
  }
  safe operator--(int) {
    auto old = *this;
    --*this;
    return old;
  }

  template <typename F>
  friend constexpr
      typename std::enable_if<std::is_floating_point<F>::value, F>::type
      operator+(safe a, F b) {
    return F(a.value_) + b;
  }

  template <typename F>
  friend constexpr
      typename std::enable_if<std::is_floating_point<F>::value, F>::type
      operator+(F a, safe b) {
    return a + F(b.value_);
  }

  template <typename F>
  friend constexpr
      typename std::enable_if<std::is_floating_point<F>::value, F>::type
      operator-(safe a, F b) {
    return F(a.value_) - b;
  }

  template <typename F>
  friend constexpr
      typename std::enable_if<std::is_floating_point<F>::value, F>::type
      operator-(F a, safe b) {
    return a - F(b.value_);
  }

  template <typename F>
  friend constexpr
      typename std::enable_if<std::is_floating_point<F>::value, F>::type
      operator*(safe a, F b) {
    return F(a.value_) * b;
  }

  template <typename F>
  friend constexpr
      typename std::enable_if<std::is_floating_point<F>::value, F>::type
      operator*(F a, safe b) {
    return a * F(b.value_);
  }

  template <typename F>
  friend constexpr
      typename std::enable_if<std::is_floating_point<F>::value, F>::type
      operator/(safe a, F b) {
    return F(a.value_) / b;
  }

  template <typename F>
  friend constexpr
      typename std::enable_if<std::is_floating_point<F>::value, F>::type
      operator/(F a, safe b) {
    return a / F(b.value_);
  }

  template <typename U>
  constexpr explicit operator U() const {
    return convert<U, T>(value_);
  }

 private:
  static constexpr T MIN = std::numeric_limits<T>::min();
  static constexpr T MAX = std::numeric_limits<T>::max();
  static constexpr T NEG_ONE = -1;

  template <typename Y, typename X>
  constexpr typename std::enable_if<std::is_integral<Y>::value &&
                                        std::is_unsigned<Y>::value &&
                                        std::is_integral<X>::value,
                                    Y>::type
  convert(X x) {
    using UX = typename std::make_unsigned<X>::type;
    return (x < 0 || UX(x) > std::numeric_limits<Y>::max())
               ? (throw std::overflow_error("result out of range"), 0)
               : UX(x);
  }

  template <typename Y, typename X>
  constexpr typename std::enable_if<
      std::is_integral<Y>::value && std::is_signed<Y>::value &&
          std::is_integral<X>::value && std::is_signed<X>::value,
      Y>::type
  convert(X x) {
    return (x < std::numeric_limits<Y>::min() ||
            x > std::numeric_limits<Y>::max())
               ? (throw std::overflow_error("result out of range"), 0)
               : x;
  }

  template <typename Y, typename X>
  constexpr typename std::enable_if<
      std::is_integral<Y>::value && std::is_signed<Y>::value &&
          std::is_integral<X>::value && std::is_unsigned<X>::value,
      Y>::type
  convert(X x) {
    using UY = typename std::make_unsigned<Y>::type;
    return (x > UY(std::numeric_limits<Y>::max()))
               ? (throw std::overflow_error("result out of range"), 0)
               : UY(x);
  }

  template <typename Y, typename X>
  constexpr typename std::enable_if<
      std::is_floating_point<Y>::value && std::is_integral<X>::value, Y>::type
  convert(X x) {
    return x;
  }

  constexpr static bool negate_is_unsafe(T x) {
    return std::is_unsigned<T>::value || x < -MAX;
  }

  constexpr static T negate(T x) {
    return negate_is_unsafe(x)
               ? (throw std::overflow_error("result out of range"), 0)
               : -x;
  }

  constexpr static T abs(T x) { return (x < 0) ? negate(x) : x; }

  constexpr static T sgn(T x) noexcept {
    return (x == 0) ? 0 : ((x < 0) ? -1 : 1);
  }

  constexpr static safe add_impl(safe a, safe b) {
    return ((b.value_ > 0 && a.value_ > MAX - b.value_) ||
            (b.value_ < 0 && a.value_ < MIN - b.value_))
               ? (throw std::overflow_error("result out of range"), 0)
               : (a.value_ + b.value_);
  }

  constexpr static safe sub_impl(safe a, safe b) {
    return ((b.value_ > 0 && a.value_ < MIN + b.value_) ||
            (b.value_ < 0 && a.value_ > MAX + b.value_))
               ? (throw std::overflow_error("result out of range"), 0)
               : (a.value_ - b.value_);
  }

  constexpr static bool mul_is_unsafe(safe a, safe b) {
    return ((a.value_ > 0 && b.value_ > 0 && a.value_ > MAX / b.value_) ||
            (a.value_ > 0 && b.value_ < 0 && b.value_ < MIN / a.value_) ||
            (a.value_ < 0 && b.value_ > 0 && a.value_ < MIN / b.value_) ||
            (a.value_ < 0 && b.value_ < 0 && a.value_ < MAX / b.value_));
  }

  constexpr static safe mul_impl(safe a, safe b) {
    return mul_is_unsafe(a, b)
               ? (throw std::overflow_error("result out of range"), 0)
               : (a.value_ * b.value_);
  }

  using Pair = std::pair<safe, safe>;

  constexpr static bool div_is_unsafe(safe a, safe b) {
    return (MIN < 0 && MIN < -MAX &&
            ((a.value_ == MIN && b.value_ == NEG_ONE) ||
             (a.value_ == NEG_ONE && b.value_ == MIN)));
  }

  constexpr static Pair divmod_impl(safe a, safe b) {
    return (b.value_ == 0)
               ? (throw std::domain_error("divide by zero"), Pair(0, 0))
               : (div_is_unsafe(a, b)
                      ? (throw std::overflow_error("result out of range"),
                         Pair(0, 0))
                      : Pair(a.value_ / b.value_, a.value_ % b.value_));
  }

  T value_;
};

}  // namespace base

#endif  // BASE_SAFEMATH_H
