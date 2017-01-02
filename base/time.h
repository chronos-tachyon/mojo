// base/time.h - Value type representing an instant of time
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_TIME_H
#define BASE_TIME_H

#include <memory>
#include <string>

#include "base/duration.h"

namespace base {

// Time represents an instant of time on a wall clock.
//
// - It is guaranteed to have nanosecond precision.
//
// - It is guaranteed to have a range equal to time_t or better.
//
// - It uses the Unix epoch (01 Jan 1970 00:00:00 UTC).
//
// - It is NOT guaranteed to move forward monotonically.
//   ~ It may go backward due to leap seconds.
//   ~ It may go backward due to the time daemon adjusting the clock.
//
class Time {
 private:
  explicit constexpr Time(Duration d) noexcept : d_(d) {}

 public:
  // Time is default constructible, copyable, and moveable.
  constexpr Time() noexcept : Time(Duration()) {}
  constexpr Time(const Time&) noexcept = default;
  constexpr Time(Time&&) noexcept = default;
  Time& operator=(const Time&) noexcept = default;
  Time& operator=(Time&&) noexcept = default;

  // Constructs a Time in terms of the Duration since the epoch.
  static constexpr Time from_epoch(Duration d) noexcept { return Time(d); }

  // Returns the Time as a Duration since the epoch.
  constexpr Duration since_epoch() const noexcept { return d_; }

  // Returns true iff this Time represents the epoch itself.
  constexpr bool is_epoch() const noexcept { return d_.is_zero(); }

  // Returns true iff this Time lies before the epoch.
  constexpr bool before_epoch() const noexcept { return d_.is_neg(); }

  // Times can be swapped.
  void swap(Time& other) noexcept { d_.swap(other.d_); }

  // Arithmetic operators, part 1 {{{

  Time& operator+=(Duration b);
  Time& operator-=(Duration b);

  // }}}

  void append_to(std::string* out) const;
  std::string as_string() const;

 private:
  Duration d_;
};

inline void swap(Time& a, Time& b) noexcept { a.swap(b); }

// Comparison operators {{{

inline constexpr bool operator==(Time a, Time b) noexcept {
  return a.since_epoch() == b.since_epoch();
}
inline constexpr bool operator!=(Time a, Time b) noexcept { return !(a == b); }
inline constexpr bool operator<(Time a, Time b) noexcept {
  return a.since_epoch() < b.since_epoch();
}
inline constexpr bool operator>(Time a, Time b) noexcept { return (b < a); }
inline constexpr bool operator<=(Time a, Time b) noexcept { return !(b < a); }
inline constexpr bool operator>=(Time a, Time b) noexcept { return !(a < b); }

// }}}
// Arithmetic operators, part 2 {{{

inline constexpr Time operator+(Time a, Duration b) {
  return Time::from_epoch(a.since_epoch() + b);
}
inline constexpr Time operator+(Duration a, Time b) {
  return Time::from_epoch(a + b.since_epoch());
}
inline constexpr Time operator-(Time a, Duration b) { return a + (-b); }
inline constexpr Duration operator-(Time a, Time b) {
  return a.since_epoch() - b.since_epoch();
}

inline Time& Time::operator+=(Duration b) { return (*this = (*this + b)); }
inline Time& Time::operator-=(Duration b) { return (*this = (*this - b)); }

// }}}

// MonotonicTime represents an instant of time on a monotonic clock.
//
// - It is guaranteed to have nanosecond precision.
//
// - It is guaranteed to have a range equal to time_t or better.
//
// - It is NOT guaranteed to have any particular epoch.
//   ~ It is NOT guaranteed to have the same epoch across application restarts.
//   ~ In particular, the monotonic clock's epoch may be something as arbitrary
//     as "time since last reboot".
//
class MonotonicTime {
 private:
  explicit constexpr MonotonicTime(Duration d) noexcept : d_(d) {}

 public:
  // MonotonicTime is default constructible, copyable, and moveable.
  constexpr MonotonicTime() noexcept : MonotonicTime(Duration()) {}
  constexpr MonotonicTime(const MonotonicTime&) noexcept = default;
  constexpr MonotonicTime(MonotonicTime&&) noexcept = default;
  MonotonicTime& operator=(const MonotonicTime&) noexcept = default;
  MonotonicTime& operator=(MonotonicTime&&) noexcept = default;

  // Constructs a MonotonicTime in terms of the Duration since the epoch.
  static constexpr MonotonicTime from_epoch(Duration d) noexcept {
    return MonotonicTime(d);
  }

  // Returns the MonotonicTime as a Duration since the epoch.
  constexpr Duration since_epoch() const noexcept { return d_; }

  // Returns true iff this MonotonicTime represents the epoch itself.
  constexpr bool is_epoch() const noexcept { return d_.is_zero(); }

  // Returns true iff this MonotonicTime lies before the epoch.
  constexpr bool before_epoch() const noexcept { return d_.is_neg(); }

  // Times can be swapped.
  void swap(MonotonicTime& other) noexcept { d_.swap(other.d_); }

  // Arithmetic operators, part 1 {{{

  MonotonicTime& operator+=(Duration b);
  MonotonicTime& operator-=(Duration b);

  // }}}

  void append_to(std::string* out) const;
  std::string as_string() const;

 private:
  Duration d_;
};

inline void swap(MonotonicTime& a, MonotonicTime& b) noexcept { a.swap(b); }

// Comparison operators {{{

inline constexpr bool operator==(MonotonicTime a, MonotonicTime b) noexcept {
  return a.since_epoch() == b.since_epoch();
}
inline constexpr bool operator!=(MonotonicTime a, MonotonicTime b) noexcept {
  return !(a == b);
}
inline constexpr bool operator<(MonotonicTime a, MonotonicTime b) noexcept {
  return a.since_epoch() < b.since_epoch();
}
inline constexpr bool operator>(MonotonicTime a, MonotonicTime b) noexcept {
  return (b < a);
}
inline constexpr bool operator<=(MonotonicTime a, MonotonicTime b) noexcept {
  return !(b < a);
}
inline constexpr bool operator>=(MonotonicTime a, MonotonicTime b) noexcept {
  return !(a < b);
}

// }}}
// Arithmetic operators, part 2 {{{

inline constexpr MonotonicTime operator+(MonotonicTime a, Duration b) {
  return MonotonicTime::from_epoch(a.since_epoch() + b);
}
inline constexpr MonotonicTime operator+(Duration a, MonotonicTime b) {
  return MonotonicTime::from_epoch(a + b.since_epoch());
}
inline constexpr MonotonicTime operator-(MonotonicTime a, Duration b) {
  return a + (-b);
}
inline constexpr Duration operator-(MonotonicTime a, MonotonicTime b) {
  return a.since_epoch() - b.since_epoch();
}

inline MonotonicTime& MonotonicTime::operator+=(Duration b) {
  return (*this = (*this + b));
}
inline MonotonicTime& MonotonicTime::operator-=(Duration b) {
  return (*this = (*this - b));
}

// }}}

}  // namespace base

#endif  // BASE_TIME_H
