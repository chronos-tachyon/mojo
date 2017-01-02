// base/clock.h - Interface for obtaining base::Time values
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_CLOCK_H
#define BASE_CLOCK_H

#include <memory>

#include "base/time.h"

namespace base {

// ClockImpl is the abstract base class for Clocks.
// It is exposed mostly for use by unit tests.
class ClockImpl {
 protected:
  ClockImpl() noexcept = default;

 public:
  // ClockImpls are neither copyable nor moveable.
  ClockImpl(const ClockImpl&) = delete;
  ClockImpl(ClockImpl&&) = delete;
  ClockImpl& operator=(const ClockImpl&) = delete;
  ClockImpl& operator=(ClockImpl&&) = delete;

  virtual ~ClockImpl() noexcept = default;

  // Obtains the current Unix-epoch wallclock time.
  //
  // THREAD SAFETY: This method MUST be thread-safe.
  //
  virtual Time now() const = 0;
};

// Clock tracks the current Unix-epoch wallclock time.
class Clock {
 public:
  // Clocks are normally constructed from an implementation.
  Clock(std::shared_ptr<const ClockImpl> ptr) noexcept : ptr_(std::move(ptr)) {}

  // Clocks are default constructible, copyable, and moveable.
  Clock() noexcept = default;
  Clock(const Clock&) noexcept = default;
  Clock(Clock&) noexcept = default;
  Clock& operator=(const Clock&) noexcept = default;
  Clock& operator=(Clock&) noexcept = default;

  // A valid Clock is one that has an implementation.
  explicit operator bool() const noexcept { return !!ptr_; }
  void assert_valid() const;

  // Obtains the current Unix-epoch wallclock time.
  //
  // THREAD SAFETY: This method is thread-safe.
  //
  Time now() const {
    assert_valid();
    return ptr_->now();
  }

 private:
  std::shared_ptr<const ClockImpl> ptr_;
};

// MonotonicClockImpl is the abstract base class for MonotonicClocks.
// It is exposed mostly for use by unit tests.
class MonotonicClockImpl {
 protected:
  MonotonicClockImpl() noexcept = default;

 public:
  // MonotonicClockImpls are neither copyable nor moveable.
  MonotonicClockImpl(const MonotonicClockImpl&) = delete;
  MonotonicClockImpl(MonotonicClockImpl&&) = delete;
  MonotonicClockImpl& operator=(const MonotonicClockImpl&) = delete;
  MonotonicClockImpl& operator=(MonotonicClockImpl&&) = delete;

  virtual ~MonotonicClockImpl() noexcept = default;

  // Obtains the current monotonic time.
  //
  // THREAD SAFETY: This method MUST be thread-safe.
  //
  virtual MonotonicTime now() const = 0;

  // Tries to convert a time in the Unix epoch into a monotonic time.
  //
  // THREAD SAFETY: This method MUST be thread-safe.
  //
  virtual MonotonicTime convert(Time t) const = 0;

  // Tries to convert a monotonic time into a time in the Unix epoch.
  //
  // THREAD SAFETY: This method MUST be thread-safe.
  //
  virtual Time convert(MonotonicTime t) const = 0;
};

// MonotonicClock tracks the current Unix-epoch wallclock time.
class MonotonicClock {
 public:
  // MonotonicClocks are normally constructed from an implementation.
  MonotonicClock(std::shared_ptr<const MonotonicClockImpl> ptr) noexcept
      : ptr_(std::move(ptr)) {}

  // MonotonicClocks are default constructible, copyable, and moveable.
  MonotonicClock() noexcept = default;
  MonotonicClock(const MonotonicClock&) noexcept = default;
  MonotonicClock(MonotonicClock&) noexcept = default;
  MonotonicClock& operator=(const MonotonicClock&) noexcept = default;
  MonotonicClock& operator=(MonotonicClock&) noexcept = default;

  // A valid MonotonicClock is one that has an implementation.
  explicit operator bool() const noexcept { return !!ptr_; }
  void assert_valid() const;

  // Obtains the current monotonic time.
  //
  // THREAD SAFETY: This method is thread-safe.
  //
  MonotonicTime now() const {
    assert_valid();
    return ptr_->now();
  }

  MonotonicTime convert(Time t) const {
    assert_valid();
    return ptr_->convert(t);
  }

  Time convert(MonotonicTime t) const {
    assert_valid();
    return ptr_->convert(t);
  }

 private:
  std::shared_ptr<const MonotonicClockImpl> ptr_;
};

// Returns a shared Clock that always reflects the current time.
//
// This clock can move backwards and express other discontinuities, but it
// tracks the current UTC time, relative to the Unix epoch.
//
// THREAD SAFETY: This function is thread-safe.
//
Clock system_wallclock();

// Returns a shared MonotonicClock that returns monotonically increasing times.
//
// This clock's rate can drift relative to wallclock time, e.g. if the system
// clock is running too fast and has to be adjusted by the time daemon.
//
// The epoch is unspecified, and it may change across application restarts.
//
// Use this clock for measuring the duration between times.
//
// THREAD SAFETY: This function is thread-safe.
//
MonotonicClock system_monotonic_clock();

// Convenience methods for obtaining the current system clock time.
//
// THREAD SAFETY: These functions are thread-safe.
//
inline Time now() { return system_wallclock().now(); }
inline MonotonicTime monotonic_now() { return system_monotonic_clock().now(); }

// Replaces the specified clock.
// These functions should only be used in unit tests.
//
// THREAD SAFETY: These functions are thread-safe.
//
void set_system_wallclock(Clock clock);
void set_system_monotonic_clock(MonotonicClock clock);

}  // namespace base

#endif  // BASE_CLOCK_H
