// base/time/clockfake.h - Fake ClockImpl for unit testing
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_TIME_CLOCKFAKE_H
#define BASE_TIME_CLOCKFAKE_H

#include "base/time/clock.h"

namespace base {
namespace time {

class FakeClock : public ClockImpl {
 private:
  static Time default_now() noexcept {
    return Time::from_epoch(Duration::from_raw(false, 1136239445U, 123456789U));
  }

 public:
  // Constructs a FakeClock with a current time of |now|.
  FakeClock(Time now) noexcept : now_(now) {}

  // Constructs a FakeClock with a current time of:
  //   Mon 2006 Jan 02 15:04:05.123456789 -0700
  FakeClock() noexcept : FakeClock(default_now()) {}

  // Returns the clock's current time.
  Time now() const noexcept override { return now_; }

  // Advances the clock's current time by |dur|.
  void add(Duration dur) { now_ += dur; }

  operator Clock() const noexcept {
    auto noop_deleter = [](const void*) {};
    return Clock(std::shared_ptr<const ClockImpl>(this, noop_deleter));
  }

 private:
  Time now_;
};

class FakeMonotonicClock : public MonotonicClockImpl {
 private:
  static MonotonicTime default_now() noexcept { return MonotonicTime(); }

  static Duration default_delta() noexcept {
    return Duration::from_raw(false, 1136239445U, 123456789U);
  }

 public:
  // Constructs a monotonic clock at the given instant.
  // - |delta| is the monotonic epoch's offset against the walltime epoch.
  FakeMonotonicClock(MonotonicTime now, Duration delta) noexcept
      : now_(now),
        delta_(delta) {}

  // Constructs a monotonic clock at the epoch.
  FakeMonotonicClock() noexcept
      : FakeMonotonicClock(default_now(), default_delta()) {}

  // Returns the clock's current time.
  MonotonicTime now() const noexcept override { return now_; }

  // Converts walltime to monotonic time.
  MonotonicTime convert(Time t) const noexcept override {
    return MonotonicTime::from_epoch(t.since_epoch() - delta_);
  }

  // Converts monotonic time to walltime.
  Time convert(MonotonicTime t) const noexcept override {
    return Time::from_epoch(t.since_epoch() + delta_);
  }

  // Advances the clock's current time by |dur|.
  void add(Duration dur) { now_ += dur; }

  operator MonotonicClock() const noexcept {
    auto noop_deleter = [](const void*) {};
    return MonotonicClock(
        std::shared_ptr<const MonotonicClockImpl>(this, noop_deleter));
  }

 private:
  MonotonicTime now_;
  Duration delta_;
};

}  // namespace time
}  // namespace base

#endif  // BASE_TIME_CLOCKFAKE_H
