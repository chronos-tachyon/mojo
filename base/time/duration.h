// base/time/duration.h - Value type representing a span of time
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_TIME_DURATION_H
#define BASE_TIME_DURATION_H

#include <sys/time.h>
#include <time.h>

#include <cmath>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include "base/result.h"
#include "base/safemath.h"

namespace base {
namespace time {

namespace internal {

static constexpr uint64_t NS_PER_S = 1000000000;
static constexpr uint64_t NS_PER_MS = 1000000;
static constexpr uint64_t NS_PER_US = 1000;

static constexpr uint64_t US_PER_S = NS_PER_S / NS_PER_US;
static constexpr uint64_t MS_PER_S = NS_PER_S / NS_PER_MS;

static constexpr uint64_t S_PER_MIN = 60;
static constexpr uint64_t S_PER_HR = 3600;

struct DurationRep {
  safe<uint64_t> s;
  safe<uint32_t> ns;
  bool neg;

  constexpr DurationRep(bool neg, safe<uint64_t> s, safe<uint32_t> ns) noexcept
      : s(s),
        ns(ns),
        neg(neg) {}

  constexpr DurationRep() noexcept : DurationRep(false, 0, 0) {}

  constexpr bool is_zero() const noexcept { return s == 0 && ns == 0; }

  constexpr DurationRep normalize() const {
    return DurationRep(neg && !is_zero(), s + safe<uint64_t>(ns / NS_PER_S),
                       ns % NS_PER_S);
  }

  constexpr bool operator==(DurationRep b) noexcept {
    return s == b.s && ns == b.ns && neg == b.neg;
  }
};

}  // namespace internal

// Duration represents the width of a span of time.
// - It is guaranteed to have nanosecond precision.
// - It is guaranteed to have a range equal to time_t or better.
class Duration {
 private:
  static constexpr uint64_t NS_PER_S = internal::NS_PER_S;
  static constexpr uint64_t NS_PER_MS = internal::NS_PER_MS;
  static constexpr uint64_t NS_PER_US = internal::NS_PER_US;

  static constexpr uint64_t US_PER_S = internal::US_PER_S;
  static constexpr uint64_t MS_PER_S = internal::MS_PER_S;

  static constexpr uint64_t S_PER_MIN = internal::S_PER_MIN;
  static constexpr uint64_t S_PER_HR = internal::S_PER_HR;

 public:
  // Duration is constructible from a DurationRep.
  // Not a stable API — use at your own risk!
  constexpr explicit Duration(internal::DurationRep rep) noexcept : rep_(rep) {}

  // Duration is default constructible, copyable, and moveable.
  constexpr Duration() noexcept : rep_() {}
  constexpr Duration(const Duration&) noexcept = default;
  constexpr Duration(Duration&&) noexcept = default;
  Duration& operator=(const Duration&) noexcept = default;
  Duration& operator=(Duration&&) noexcept = default;

  // Helper for constructing a Duration from its raw components.
  // Not a stable API — use at your own risk!
  constexpr static Duration from_raw(internal::DurationRep rep) {
    return Duration(rep.normalize());
  }
  constexpr static Duration from_raw(bool neg, safe<uint64_t> s,
                                     safe<uint32_t> ns) {
    return from_raw(internal::DurationRep(neg, s, ns));
  }

  // Returns the raw components of a Duration.
  // Not a stable API — use at your own risk!
  constexpr internal::DurationRep raw() const noexcept { return rep_; }

  // Returns the smallest possible finite Duration.
  constexpr static Duration min() noexcept {
    return Duration(internal::DurationRep(
        true, std::numeric_limits<uint64_t>::max(), NS_PER_S - 1));
  }

  // Returns the largest possible finite Duration.
  constexpr static Duration max() noexcept {
    return Duration(internal::DurationRep(
        false, std::numeric_limits<uint64_t>::max(), NS_PER_S - 1));
  }

  // Returns true iff this Duration is non-zero.
  constexpr explicit operator bool() const noexcept {
    return rep_.s || rep_.ns;
  }

  // Returns true iff this is the zero Duration.
  constexpr bool is_zero() const noexcept {
    return rep_.s == 0 && rep_.ns == 0;
  }

  // Returns true iff this Duration is less than the zero Duration.
  constexpr bool is_neg() const noexcept { return rep_.neg; }

  // Returns the absolute value of this Duration.
  constexpr Duration abs() const noexcept {
    return from_raw(false, rep_.s, rep_.ns);
  }

  // Returns the sign of this Duration.
  constexpr int sgn() const noexcept {
    return is_zero() ? 0 : (rep_.neg ? -1 : 1);
  }

  // Swap two Durations.
  void swap(Duration& other) noexcept {
    using std::swap;
    swap(rep_, other.rep_);
  }

  // Comparison operators {{{

  friend constexpr bool operator==(Duration a, Duration b) noexcept {
    return a.rep_ == b.rep_;
  }
  friend constexpr bool operator!=(Duration a, Duration b) noexcept {
    return !(a == b);
  }

  friend constexpr bool operator<(Duration a, Duration b) noexcept {
    return less(a.rep_, b.rep_);
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
    return from_raw(negate(rep_));
  }

  friend constexpr Duration operator+(Duration a, Duration b) {
    return from_raw(add(a.rep_, b.rep_));
  }

  Duration& operator+=(Duration b) { return (*this = (*this + b)); }

  friend constexpr Duration operator-(Duration a, Duration b) {
    return from_raw(sub(a.rep_, b.rep_));
  }

  Duration& operator-=(Duration b) { return (*this = (*this - b)); }

  friend Duration operator*(Duration a, safe<uint64_t> b);
  friend Duration operator*(Duration a, safe<int64_t> b);
  friend Duration operator*(Duration a, double b);

  template <typename T>
  friend typename std::enable_if<
      std::is_integral<T>::value && std::is_unsigned<T>::value, Duration>::type
  operator*(Duration a, T b) {
    return a * safe<uint64_t>(b);
  }

  template <typename T>
  friend typename std::enable_if<
      std::is_integral<T>::value && std::is_signed<T>::value, Duration>::type
  operator*(Duration a, T b) {
    return a * safe<int64_t>(b);
  }

  template <typename T>
  friend Duration operator*(T a, Duration b) {
    return b * a;
  }

  template <typename T>
  Duration& operator*=(T b) {
    return (*this = (*this * b));
  }

  friend Duration operator/(Duration a, safe<uint64_t> b);
  friend Duration operator/(Duration a, safe<int64_t> b);
  friend Duration operator/(Duration a, double b);

  template <typename T>
  friend typename std::enable_if<
      std::is_integral<T>::value && std::is_unsigned<T>::value, Duration>::type
  operator/(Duration a, T b) {
    return a / safe<uint64_t>(b);
  }

  template <typename T>
  friend typename std::enable_if<
      std::is_integral<T>::value && std::is_signed<T>::value, Duration>::type
  operator/(Duration a, T b) {
    return a / safe<int64_t>(b);
  }

  friend std::pair<double, Duration> divmod(Duration a, Duration b);
  friend double operator/(Duration a, Duration b);
  friend Duration operator%(Duration a, Duration b) {
    return divmod(a, b).second;
  }

  template <typename T>
  Duration& operator/=(T b) {
    return (*this = (*this / b));
  }

  Duration& operator%=(Duration b) { return (*this = (*this % b)); }

  // }}}

  constexpr uint64_t abs_nanoseconds() const { return uint64_t(u_ns()); }
  constexpr uint64_t abs_microseconds() const { return uint64_t(u_us()); }
  constexpr uint64_t abs_milliseconds() const { return uint64_t(u_ms()); }
  constexpr uint64_t abs_seconds() const { return uint64_t(u_s()); }
  constexpr uint64_t abs_minutes() const { return uint64_t(u_min()); }
  constexpr uint64_t abs_hours() const { return uint64_t(u_hr()); }

  constexpr int64_t nanoseconds() const { return sgn() * int64_t(u_ns()); }
  constexpr int64_t microseconds() const { return sgn() * int64_t(u_us()); }
  constexpr int64_t milliseconds() const { return sgn() * int64_t(u_ms()); }
  constexpr int64_t seconds() const { return sgn() * int64_t(u_s()); }
  constexpr int64_t minutes() const { return sgn() * int64_t(u_min()); }
  constexpr int64_t hours() const { return sgn() * int64_t(u_hr()); }

  constexpr double fnanoseconds() const { return sgn() * f_ns(); }
  constexpr double fmicroseconds() const { return sgn() * f_us(); }
  constexpr double fmilliseconds() const { return sgn() * f_ms(); }
  constexpr double fseconds() const { return sgn() * f_s(); }
  constexpr double fminutes() const { return sgn() * f_min(); }
  constexpr double fhours() const { return sgn() * f_hr(); }

  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept;
  std::string as_string() const;

 private:
  using Rep = internal::DurationRep;
  using U64 = safe<uint64_t>;
  using S64 = safe<int64_t>;
  using D = double;

  constexpr static bool less_p(Rep a, Rep b) noexcept {
    return a.s < b.s || (a.s == b.s && a.ns < b.ns);
  }

  constexpr static bool less(Rep a, Rep b) noexcept {
    return (a.neg && b.neg) ? less_p(b, a)
                            : ((!a.neg && !b.neg) ? less_p(a, b) : a.neg);
  }

  constexpr static Rep negate(Rep a) { return Rep(!a.neg, a.s, a.ns); }

  constexpr static Rep add_p(Rep a, Rep b) {
    return Rep(a.neg, a.s + b.s, a.ns + b.ns);
  }

  constexpr static Rep sub_ns(Rep a, Rep b) {
    return (a.ns < b.ns) ? Rep(!a.neg, 0, b.ns - a.ns)
                         : Rep(a.neg, 0, a.ns - b.ns);
  }

  constexpr static Rep sub_s(Rep a, Rep b) {
    // (k*s + ns) - (k*b.s + b.ns)
    // k*(s - b.s) + (ns - b.ns)
    // k*(s - b.s) + (ns + k - b.ns) - k
    // k*(s - b.s - 1) + (ns + k - b.ns)
    using namespace internal;
    return (a.ns < b.ns) ? Rep(a.neg, (a.s - b.s - 1), (a.ns + NS_PER_S - b.ns))
                         : Rep(a.neg, (a.s - b.s), (a.ns - b.ns));
  }

  constexpr static Rep sub_p(Rep a, Rep b) {
    return ((a.s == b.s) ? sub_ns(a, b)
                         : ((a.s < b.s) ? negate(sub_s(b, a)) : sub_s(a, b)));
  }

  constexpr static Rep add(Rep a, Rep b) {
    return (a.neg == b.neg) ? add_p(a, b) : sub_p(a, negate(b));
  }

  constexpr static Rep sub(Rep a, Rep b) {
    return (a.neg == b.neg) ? sub_p(a, b) : add_p(a, negate(b));
  }

  constexpr U64 u_ns() const { return rep_.s * NS_PER_S + U64(rep_.ns); }
  constexpr U64 u_us() const {
    return rep_.s * US_PER_S + U64(rep_.ns) / NS_PER_US;
  }
  constexpr U64 u_ms() const {
    return rep_.s * MS_PER_S + U64(rep_.ns) / NS_PER_MS;
  }
  constexpr U64 u_s() const { return rep_.s; }
  constexpr U64 u_min() const { return rep_.s / S_PER_MIN; }
  constexpr U64 u_hr() const { return rep_.s / S_PER_HR; }

  constexpr D f_ns() const { return D(rep_.s) * NS_PER_S + D(rep_.ns); }
  constexpr D f_us() const {
    return D(rep_.s) * US_PER_S + D(rep_.ns) / NS_PER_US;
  }
  constexpr D f_ms() const {
    return D(rep_.s) * MS_PER_S + D(rep_.ns) / NS_PER_MS;
  }
  constexpr D f_s() const { return D(rep_.s) + D(rep_.ns) / NS_PER_S; }
  constexpr D f_min() const { return f_s() / S_PER_MIN; }
  constexpr D f_hr() const { return f_s() / S_PER_HR; }

  Rep rep_;
};

inline void swap(Duration& a, Duration& b) noexcept { a.swap(b); }

std::ostream& operator<<(std::ostream& o, Duration d);

// Constructors for Duration {{{

extern const Duration NANOSECOND;
extern const Duration MICROSECOND;
extern const Duration MILLISECOND;
extern const Duration SECOND;
extern const Duration MINUTE;
extern const Duration HOUR;

template <typename T>
Duration nanoseconds(T scale) {
  return scale * NANOSECOND;
}

template <typename T>
Duration microseconds(T scale) {
  return scale * MICROSECOND;
}

template <typename T>
Duration milliseconds(T scale) {
  return scale * MILLISECOND;
}

template <typename T>
Duration seconds(T scale) {
  return scale * SECOND;
}

template <typename T>
Duration minutes(T scale) {
  return scale * MINUTE;
}

template <typename T>
Duration hours(T scale) {
  return scale * HOUR;
}

// }}}

Result duration_from_timeval(Duration* out, const struct timeval* tv);
Result duration_from_timespec(Duration* out, const struct timespec* ts);

Result timeval_from_duration(struct timeval* out, Duration dur);
Result timespec_from_duration(struct timespec* out, Duration dur);

}  // namespace time
}  // namespace base

#endif  // BASE_TIME_DURATION_H
