// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/clock.h"

#include <time.h>

#include <cstring>
#include <mutex>
#include <stdexcept>
#include <system_error>

#include "base/time/duration.h"
#include "base/time/time.h"

namespace base {
namespace time {

void Clock::assert_valid() const {
  if (!ptr_) throw std::logic_error("base::time::Clock is empty");
}

void MonotonicClock::assert_valid() const {
  if (!ptr_) throw std::logic_error("base::time::MonotonicClock is empty");
}

class SystemWallClock : public ClockImpl {
 public:
  SystemWallClock() noexcept = default;
  Time now() const override {
    struct timespec ts;
    ::bzero(&ts, sizeof(ts));
    int rc = clock_gettime(CLOCK_REALTIME, &ts);
    if (rc != 0) {
      int err_no = errno;
      throw std::system_error(err_no, std::system_category(),
                              "clock_gettime(2)");
    }
    return Time::from_epoch(Duration::from_raw(false, ts.tv_sec, ts.tv_nsec));
  }
};

static Duration compute_wall_minus_mono() {
  struct timespec ts0, ts1, ts2;
  ::bzero(&ts0, sizeof(ts0));
  ::bzero(&ts1, sizeof(ts1));
  ::bzero(&ts2, sizeof(ts2));
  clock_gettime(CLOCK_MONOTONIC, &ts0);
  clock_gettime(CLOCK_REALTIME, &ts1);
  clock_gettime(CLOCK_MONOTONIC, &ts2);

  ts2.tv_sec -= ts0.tv_sec;
  while (ts2.tv_nsec < ts0.tv_nsec) {
    --ts2.tv_sec;
    ts2.tv_nsec += internal::NANO_PER_SEC;
  }
  ts2.tv_nsec -= ts0.tv_nsec;
  uint64_t ns =
      uint64_t(ts2.tv_sec) * internal::NANO_PER_SEC + uint64_t(ts2.tv_nsec);
  ns /= 2;

  Duration mt =
      Duration::from_raw(false, ts0.tv_sec, ts0.tv_nsec) + nanoseconds(ns);
  Duration wt = Duration::from_raw(false, ts1.tv_sec, ts1.tv_nsec);
  return wt - mt;
}

static Duration wall_minus_mono() {
  static Duration d = compute_wall_minus_mono();
  return d;
}

class SystemMonotonicClock : public MonotonicClockImpl {
 public:
  SystemMonotonicClock() noexcept = default;
  MonotonicTime now() const override {
    struct timespec ts;
    ::bzero(&ts, sizeof(ts));
    int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (rc != 0) {
      int err_no = errno;
      throw std::system_error(err_no, std::system_category(),
                              "clock_gettime(2)");
    }
    return MonotonicTime::from_epoch(
        Duration::from_raw(false, ts.tv_sec, ts.tv_nsec));
  }
  MonotonicTime convert(Time t) const override {
    return MonotonicTime::from_epoch(t.since_epoch() - wall_minus_mono());
  }
  Time convert(MonotonicTime t) const override {
    return Time::from_epoch(t.since_epoch() + wall_minus_mono());
  }
};

static std::mutex g_sysclk_mu;
static Clock* g_sysclk_wall = nullptr;
static MonotonicClock* g_sysclk_mono = nullptr;

static void initialize_clocks() {
  if (g_sysclk_wall == nullptr) g_sysclk_wall = new Clock;
  if (!*g_sysclk_wall) {
    *g_sysclk_wall = Clock(std::make_shared<SystemWallClock>());
  }
  if (g_sysclk_mono == nullptr) g_sysclk_mono = new MonotonicClock;
  if (!*g_sysclk_mono) {
    *g_sysclk_mono = MonotonicClock(std::make_shared<SystemMonotonicClock>());
  }
}

Clock system_wallclock() {
  std::unique_lock<std::mutex> lock(g_sysclk_mu);
  initialize_clocks();
  return *g_sysclk_wall;
}

MonotonicClock system_monotonic_clock() {
  std::unique_lock<std::mutex> lock(g_sysclk_mu);
  initialize_clocks();
  return *g_sysclk_mono;
}

void set_system_wallclock(Clock clock) {
  std::unique_lock<std::mutex> lock(g_sysclk_mu);
  initialize_clocks();
  *g_sysclk_wall = std::move(clock);
}

void set_system_monotonic_clock(MonotonicClock clock) {
  std::unique_lock<std::mutex> lock(g_sysclk_mu);
  initialize_clocks();
  *g_sysclk_mono = std::move(clock);
}

}  // namespace time
}  // namespace base
