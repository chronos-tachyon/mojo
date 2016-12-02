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

  // Obtains the current time.
  //
  // THREAD SAFETY: This method MUST be thread-safe.
  //
  virtual Time now() const = 0;
};

// Clock tracks the current time.
class Clock {
 public:
  // Clocks are normally constructed from an implementation.
  Clock(std::shared_ptr<ClockImpl> ptr) noexcept : ptr_(std::move(ptr)) {}

  // Clocks are default constructible, copyable, and moveable.
  Clock() noexcept = default;
  Clock(const Clock&) noexcept = default;
  Clock(Clock&) noexcept = default;
  Clock& operator=(const Clock&) noexcept = default;
  Clock& operator=(Clock&) noexcept = default;

  // A valid Clock is one that has an implementation.
  explicit operator bool() const noexcept { return !!ptr_; }
  bool valid() const noexcept { return !!*this; }
  void assert_valid() const;

  // Retrieves the current time.
  //
  // THREAD SAFETY: This method is thread-safe.
  //
  Time now() const {
    assert_valid();
    return ptr_->now();
  }

 private:
  std::shared_ptr<ClockImpl> ptr_;
};

// Returns a shared Clock that always reflects the current time.
//
// This clock can move backwards and express other discontinuities, but it
// probably tracks the official UTC time.
//
// Use it for finding the current UTC time, relative to the Unix epoch.
//
// THREAD SAFETY: This function is thread-safe.
//
Clock system_wallclock();

// Returns a shared Clock that returns monotonically increasing values.
//
// This clock can drift from wallclock time, e.g. if the system clock is
// running too fast and has to be adjusted by the time daemon.
//
// The epoch is not defined, and the epoch may change across system reboots.
//
// Use this clock for measuring the duration between times.
//
// THREAD SAFETY: This function is thread-safe.
//
Clock system_monotonic_clock();

// Convenience methods for obtaining the current system clock time.
//
// THREAD SAFETY: These functions are thread-safe.
//
inline Time wallclock_now() { return system_wallclock().now(); }
inline Time monotonic_now() { return system_monotonic_clock().now(); }

// Replaces the specified clock.
// These functions should only be used in unit tests.
//
// THREAD SAFETY: These functions are thread-safe.
//
void set_system_wallclock(Clock clock);
void set_system_monotonic_clock(Clock clock);

}  // namespace base

#endif  // BASE_CLOCK_H
