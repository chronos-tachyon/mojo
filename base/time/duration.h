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

constexpr int32_t NANO_PER_SEC = 1000000000;
constexpr int32_t NANO_PER_MILLI = 1000000;
constexpr int32_t NANO_PER_MICRO = 1000;

constexpr int32_t MICRO_PER_SEC = NANO_PER_SEC / NANO_PER_MICRO;
constexpr int32_t MILLI_PER_SEC = NANO_PER_SEC / NANO_PER_MILLI;

constexpr int32_t SEC_PER_MIN = 60;
constexpr int32_t MIN_PER_HOUR = 60;
constexpr int32_t HOUR_PER_DAY = 24;
constexpr int32_t MONTH_PER_YEAR = 12;

constexpr int32_t SEC_PER_HOUR = SEC_PER_MIN * MIN_PER_HOUR;
constexpr int32_t SEC_PER_DAY = SEC_PER_HOUR * HOUR_PER_DAY;

constexpr int32_t DAY_PER_YEAR = 365;
constexpr int32_t DAY_PER_4YEAR = 365 * 4 + 1;
constexpr int32_t DAY_PER_100YEAR = 365 * 100 + 24;
constexpr int32_t DAY_PER_400YEAR = 365 * 400 + 97;

constexpr uint64_t SEC_MAX = std::numeric_limits<uint64_t>::max();
constexpr uint32_t NANO_MAX = NANO_PER_SEC - 1;

// 719,527 days from 0000-01-01 to 1970-01-01
//
//                 Days   Year
//                    0      0
//    + 146097 = 146097    400
//    + 146097 = 292194    800
//    + 146097 = 438291   1200
//    + 146097 = 584388   1600
//    +  36524 = 620912   1700
//    +  36524 = 657436   1800
//    +  36524 = 693960   1900
//    +   7305 = 701265   1920
//    +   7305 = 708570   1940
//    +   7305 = 715875   1960
//    +   1461 = 717336   1964
//    +   1461 = 718797   1968
//    +    365 = 719162   1969
//    +    365 = 719527   1970
//
constexpr int32_t Y1970 = 719527;

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
    return DurationRep(neg && !is_zero(), s + safe<uint64_t>(ns / NANO_PER_SEC),
                       ns % NANO_PER_SEC);
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
  static constexpr uint64_t NANO_PER_SEC = internal::NANO_PER_SEC;
  static constexpr uint64_t NANO_PER_MILLI = internal::NANO_PER_MILLI;
  static constexpr uint64_t NANO_PER_MICRO = internal::NANO_PER_MICRO;

  static constexpr uint64_t MICRO_PER_SEC = internal::MICRO_PER_SEC;
  static constexpr uint64_t MILLI_PER_SEC = internal::MILLI_PER_SEC;

  static constexpr uint64_t SEC_PER_MIN = internal::SEC_PER_MIN;
  static constexpr uint64_t SEC_PER_HOUR = internal::SEC_PER_HOUR;

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
    using namespace internal;
    return Duration(internal::DurationRep(true, SEC_MAX, NANO_MAX));
  }

  // Returns the largest possible finite Duration.
  constexpr static Duration max() noexcept {
    using namespace internal;
    return Duration(internal::DurationRep(false, SEC_MAX, NANO_MAX));
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
    return (a.ns < b.ns)
               ? Rep(a.neg, (a.s - b.s - 1), (a.ns + NANO_PER_SEC - b.ns))
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

  constexpr U64 u_ns() const { return rep_.s * NANO_PER_SEC + U64(rep_.ns); }
  constexpr U64 u_us() const {
    return rep_.s * MICRO_PER_SEC + U64(rep_.ns) / NANO_PER_MICRO;
  }
  constexpr U64 u_ms() const {
    return rep_.s * MILLI_PER_SEC + U64(rep_.ns) / NANO_PER_MILLI;
  }
  constexpr U64 u_s() const { return rep_.s; }
  constexpr U64 u_min() const { return rep_.s / SEC_PER_MIN; }
  constexpr U64 u_hr() const { return rep_.s / SEC_PER_HOUR; }

  constexpr D f_ns() const { return D(rep_.s) * NANO_PER_SEC + D(rep_.ns); }
  constexpr D f_us() const {
    return D(rep_.s) * MICRO_PER_SEC + D(rep_.ns) / NANO_PER_MICRO;
  }
  constexpr D f_ms() const {
    return D(rep_.s) * MILLI_PER_SEC + D(rep_.ns) / NANO_PER_MILLI;
  }
  constexpr D f_s() const { return D(rep_.s) + D(rep_.ns) / NANO_PER_SEC; }
  constexpr D f_min() const { return f_s() / SEC_PER_MIN; }
  constexpr D f_hr() const { return f_s() / SEC_PER_HOUR; }

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
