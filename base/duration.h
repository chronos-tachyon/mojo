// base/duration.h - Value type representing a span of time
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_DURATION_H
#define BASE_DURATION_H

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>

namespace base {

namespace internal {

static constexpr uint64_t U64MAX = std::numeric_limits<uint64_t>::max();
static constexpr uint64_t S64MAX = std::numeric_limits<int64_t>::max();

static constexpr uint64_t NS_PER_S = 1000000000;
static constexpr uint64_t NS_PER_MS = 1000000;
static constexpr uint64_t NS_PER_US = 1000;

static constexpr uint64_t S_PER_MIN = 60;
static constexpr uint64_t S_PER_HOUR = 3600;

inline constexpr uint64_t safe_add(uint64_t a, uint64_t b) {
  return (a > U64MAX - b)
             ? (throw std::overflow_error("add out of range"), U64MAX)
             : (a + b);
}

inline constexpr uint64_t safe_sub(uint64_t a, uint64_t b) {
  return (a < b) ? (throw std::underflow_error("subtract out of range"), 0)
                 : (a - b);
}

inline constexpr uint64_t safe_mul(uint64_t a, uint64_t b) {
  return (a > U64MAX / b)
             ? (throw std::overflow_error("multiply out of range"), U64MAX)
             : (a * b);
}

inline constexpr uint64_t safe_div(uint64_t a, uint64_t b) {
  return (b == 0) ? (throw std::domain_error("divide by zero"), U64MAX)
                  : (a / b);
}

inline constexpr uint64_t safe_mod(uint64_t a, uint64_t b) {
  return (b == 0) ? (throw std::domain_error("divide by zero"), U64MAX)
                  : (a % b);
}

inline constexpr int64_t safe_s64(uint64_t x) {
  return (x > S64MAX)
             ? (throw std::overflow_error("beyond int64_t range"), S64MAX)
             : x;
}

}  // namespace internal

// Duration represents the width of a span of time.
// - It is guaranteed to have nanosecond precision.
// - It is guaranteed to have a range equal to time_t or better.
struct Duration {
 private:
  explicit constexpr Duration(bool neg, uint64_t s, uint32_t ns) noexcept
      : s_(s),
        ns_(ns),
        neg_(neg) {}

  static constexpr Duration normalize1(bool neg, uint64_t s,
                                       uint64_t ns) noexcept {
    return Duration(neg && (s != 0 || ns != 0), s, ns);
  }

  static constexpr Duration normalize(bool neg, uint64_t s, uint64_t ns) {
    using namespace internal;
    return normalize1(neg, safe_add(s, safe_div(ns, NS_PER_S)),
                      safe_mod(ns, NS_PER_S));
  }

  static constexpr bool less(uint64_t as, uint64_t bs, uint32_t ans,
                             uint32_t bns) noexcept {
    return as < bs || (as == bs && ans < bns);
  }

  static constexpr bool less(bool aneg, bool bneg, uint64_t as, uint64_t bs,
                             uint32_t ans, uint32_t bns) noexcept {
    // -5 -4  -> (4 < 5) -> true
    // -5  4  -> true
    //  5 -4  -> false
    //  5  4  -> (5 < 4) -> false
    return (aneg && !bneg) || (aneg && less(bs, as, bns, ans)) ||
           less(as, bs, ans, bns);
  }

  static constexpr Duration add(bool neg, uint64_t as, uint64_t bs,
                                uint64_t ans, uint64_t bns) {
    // -5 -4  -> -9
    //  5  4  ->  9
    using namespace internal;
    return normalize(neg, safe_add(as, bs), safe_add(ans, bns));
  }

  static constexpr Duration sub_ns(uint64_t ans, uint64_t bns) {
    // 5 4  ->  1
    // 4 5  -> -1
    using namespace internal;
    return ((ans < bns) ? normalize(true, 0, safe_sub(bns, ans))
                        : normalize(false, 0, safe_sub(ans, bns)));
  }

  static constexpr Duration sub_s(uint64_t as, uint64_t bs, uint64_t ans,
                                  uint64_t bns) {
    // (k*as + ans) - (k*bs + bns)
    // k*(as - bs) + (ans - bns)
    // k*(as - bs) + (ans + k - bns) - k
    // k*(as - bs - 1) + (ans + k - bns)
    using namespace internal;
    return (ans < bns) ? normalize(false, safe_sub(safe_sub(as, bs), 1),
                                   safe_sub(safe_add(ans, NS_PER_S), bns))
                       : normalize(false, safe_sub(as, bs), safe_sub(ans, bns));
  }

  static constexpr Duration sub(uint64_t as, uint64_t bs, uint64_t ans,
                                uint64_t bns) {
    return ((as == bs) ? sub_ns(ans, bns)
                       : ((as < bs) ? -sub_s(bs, as, bns, ans)
                                    : sub_s(as, bs, ans, bns)));
  }

 public:
  // Duration is default constructible, copyable, and moveable.
  constexpr Duration() : Duration(false, 0, 0) {}
  constexpr Duration(const Duration&) noexcept = default;
  constexpr Duration(Duration&&) noexcept = default;
  Duration& operator=(const Duration&) noexcept = default;
  Duration& operator=(Duration&&) noexcept = default;

  // Helper for constructing a Duration from its raw components.
  // Not a stable API — use at your own risk!
  static constexpr Duration raw(bool neg, uint64_t s, uint64_t ns) {
    return normalize(neg, s, ns);
  }

  // Helper for constructing a Duration from its raw components.
  // Not a stable API — use at your own risk!
  static constexpr Duration raw(int64_t s, uint64_t ns) {
    return normalize(s < 0, s >= 0 ? s : -s, ns);
  }

  // Returns the raw components of a Duration.
  // Not a stable API — use at your own risk!
  constexpr std::tuple<bool, uint64_t, uint32_t> raw() const noexcept {
    return std::make_tuple(neg_, s_, ns_);
  }

  // Returns true iff this is the zero Duration.
  constexpr bool is_zero() const noexcept { return s_ == 0 && ns_ == 0; }

  // Returns true iff this Duration is less than the zero Duration.
  constexpr bool is_neg() const noexcept { return neg_; }

  // Swap two Durations.
  void swap(Duration& other) noexcept {
    using std::swap;
    swap(s_, other.s_);
    swap(ns_, other.ns_);
    swap(neg_, other.neg_);
  }

  // Comparison operators {{{

  friend constexpr bool operator==(Duration a, Duration b) noexcept {
    return a.s_ == b.s_ && a.ns_ == b.ns_ && a.neg_ == b.neg_;
  }
  friend constexpr bool operator!=(Duration a, Duration b) noexcept {
    return !(a == b);
  }

  friend constexpr bool operator<(Duration a, Duration b) noexcept {
    return less(a.neg_, b.neg_, a.s_, b.s_, a.ns_, b.ns_);
  }
  friend constexpr bool operator>(Duration a, Duration b) noexcept {
    return (b < a);
  }
  friend constexpr bool operator<=(Duration a, Duration b) noexcept {
    return !(b < a);
  }
  friend constexpr bool operator>=(Duration a, Duration b) noexcept {
    return !(a < b);
  }

  // }}}
  // Arithmetic operators {{{

  constexpr Duration operator+() const noexcept { return *this; }
  constexpr Duration operator-() const noexcept {
    return normalize1(!neg_, s_, ns_);
  }

  friend constexpr Duration operator+(Duration a, Duration b) {
    // +5 +4  -> +(5 + 4) -> +9
    // -5 -4  -> -(5 + 4) -> -9
    // -5 +4  -> -(5 - 4) -> -1
    // +5 -4  -> +(5 - 4) -> +1
    return ((a.neg_ == b.neg_) ? add(a.neg_, a.s_, b.s_, a.ns_, b.ns_)
                               : (a.neg_ ? -sub(a.s_, b.s_, a.ns_, b.ns_)
                                         : sub(a.s_, b.s_, a.ns_, b.ns_)));
  }

  Duration& operator+=(Duration b) { return (*this = (*this + b)); }

  friend constexpr Duration operator-(Duration a, Duration b) {
    return a + (-b);
  }

  Duration& operator-=(Duration b) { return (*this = (*this - b)); }

  template <typename U>
  friend constexpr
      typename std::enable_if<std::is_unsigned<U>::value, Duration>::type
      operator*(Duration a, U b) {
    // b * (s + k*ns) -> b*s + k*b*ns
    using namespace internal;
    return normalize(a.neg_, safe_mul(a.s_, b), safe_mul(a.ns_, b));
  }

  template <typename S>
  friend constexpr
      typename std::enable_if<std::is_signed<S>::value, Duration>::type
      operator*(Duration a, S b) {
    using U = typename std::make_unsigned<S>::type;
    return ((b < 0) ? -(a * static_cast<U>(-b)) : (a * static_cast<U>(b)));
  }

  template <typename T>
  friend constexpr
      typename std::enable_if<std::is_integral<T>::value, Duration>::type
      operator*(T a, Duration b) {
    return b * a;
  }

  template <typename T>
  typename std::enable_if<std::is_integral<T>::value, Duration&>::type
  operator*=(T b) {
    return (*this = (*this * b));
  }

#if 0
  template <typename U>
  friend constexpr
      typename std::enable_if<std::is_unsigned<U>::value, Duration>::type
      operator/(Duration a, U b) {
    return TODO;
  }

  template <typename S>
  friend constexpr
      typename std::enable_if<std::is_unsigned<S>::value, Duration>::type
      operator/(Duration a, S b) {
    using U = typename std::make_unsigned<S>::type;
    return (b < 0) ? -(a / U(-b)) : (a / U(b));
  }

  template <typename T>
  typename std::enable_if<std::is_integral<T>::value, Duration&>::type
  operator/=(T b) { return (*this = (*this / b)); }

  constexpr std::pair<int64_t, Duration> divmod(Duration a, Duration b) {
    return TODO;
  }

  friend constexpr int64_t operator/(Duration a, Duration b) {
    return divmod(a, b).first;
  }

  friend constexpr Duration operator%(Duration a, Duration b) {
    return divmod(a, b).second;
  }

  Duration& operator%=(Duration b) { return (*this = (*this % b)); }
#endif

  // }}}

  constexpr uint64_t abs_nanoseconds() const {
    using namespace internal;
    return safe_add(safe_mul(s_, NS_PER_S), ns_);
  }
  constexpr uint64_t abs_microseconds() const {
    using namespace internal;
    return safe_add(safe_mul(s_, safe_div(NS_PER_S, NS_PER_US)),
                    safe_div(ns_, NS_PER_US));
  }
  constexpr uint64_t abs_milliseconds() const {
    using namespace internal;
    return safe_add(safe_mul(s_, safe_div(NS_PER_S, NS_PER_MS)),
                    safe_div(ns_, NS_PER_MS));
  }
  constexpr uint64_t abs_seconds() const { return s_; }
  constexpr uint64_t abs_minutes() const {
    using namespace internal;
    return safe_div(abs_seconds(), internal::S_PER_MIN);
  }
  constexpr uint64_t abs_hours() const {
    using namespace internal;
    return safe_div(abs_seconds(), internal::S_PER_HOUR);
  }

  constexpr int64_t nanoseconds() const {
    using namespace internal;
    return neg_ ? -safe_s64(abs_nanoseconds()) : safe_s64(abs_nanoseconds());
  }
  constexpr int64_t microseconds() const {
    using namespace internal;
    return neg_ ? -safe_s64(abs_microseconds()) : safe_s64(abs_microseconds());
  }
  constexpr int64_t milliseconds() const {
    using namespace internal;
    return neg_ ? -safe_s64(abs_milliseconds()) : safe_s64(abs_milliseconds());
  }
  constexpr int64_t seconds() const {
    using namespace internal;
    return neg_ ? -safe_s64(abs_seconds()) : safe_s64(abs_seconds());
  }
  constexpr int64_t minutes() const {
    using namespace internal;
    return neg_ ? -safe_s64(abs_minutes()) : safe_s64(abs_minutes());
  }
  constexpr int64_t hours() const {
    using namespace internal;
    return neg_ ? -safe_s64(abs_hours()) : safe_s64(abs_hours());
  }

  void append_to(std::string* out) const;
  std::string as_string() const;

 private:
  uint64_t s_;
  uint32_t ns_;
  bool neg_;
};

inline void swap(Duration& a, Duration& b) noexcept { a.swap(b); }

// Constructors for Duration {{{

constexpr Duration nanoseconds(int64_t ns) {
  return Duration::raw(ns < 0, 0, ns >= 0 ? ns : -ns);
}

constexpr Duration microseconds(int64_t us) {
  return Duration::raw(us < 0, 0, internal::safe_mul(us >= 0 ? us : -us, 1000));
}

constexpr Duration milliseconds(int64_t ms) {
  return Duration::raw(ms < 0, 0,
                       internal::safe_mul(ms >= 0 ? ms : -ms, 1000000));
}

constexpr Duration seconds(int64_t s) {
  return Duration::raw(s < 0, s >= 0 ? s : -s, 0);
}

constexpr Duration minutes(int64_t min) {
  return Duration::raw(min < 0, internal::safe_mul(min >= 0 ? min : -min, 60),
                       0);
}

constexpr Duration hours(int64_t hr) {
  return Duration::raw(hr < 0, internal::safe_mul(hr >= 0 ? hr : -hr, 3600), 0);
}

// }}}

}  // namespace base

#endif  // BASE_DURATION_H
