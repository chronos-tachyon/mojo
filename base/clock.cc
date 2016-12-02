// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/clock.h"

#include <time.h>

#include <cstring>
#include <mutex>
#include <stdexcept>
#include <system_error>

#include "base/duration.h"
#include "base/time.h"

namespace base {

void Clock::assert_valid() const {
  if (!ptr_) throw std::logic_error("base::Clock is empty");
}

class SystemClock : public ClockImpl {
 public:
  explicit SystemClock(clockid_t id) noexcept : id_(id) {}

  Time now() const override;

 private:
  clockid_t id_;
};

Time SystemClock::now() const {
  struct timespec ts;
  ::bzero(&ts, sizeof(ts));
  int rc = clock_gettime(id_, &ts);
  if (rc != 0) {
    int err_no = errno;
    throw std::system_error(err_no, std::system_category(), "clock_gettime(2)");
  }
  return Time::from_epoch(Duration::raw(false, ts.tv_sec, ts.tv_nsec));
}

static std::mutex g_sysclk_mu;
static Clock* g_sysclk_wall = nullptr;
static Clock* g_sysclk_mono = nullptr;

static void initialize_clocks() {
  if (g_sysclk_wall == nullptr) g_sysclk_wall = new Clock;
  if (g_sysclk_mono == nullptr) g_sysclk_mono = new Clock;
  if (!*g_sysclk_wall) {
    *g_sysclk_wall = Clock(std::make_shared<SystemClock>(CLOCK_REALTIME));
  }
  if (!*g_sysclk_mono) {
    *g_sysclk_mono = Clock(std::make_shared<SystemClock>(CLOCK_MONOTONIC));
  }
}

Clock system_wallclock() {
  std::unique_lock<std::mutex> lock(g_sysclk_mu);
  initialize_clocks();
  return *g_sysclk_wall;
}

Clock system_monotonic_clock() {
  std::unique_lock<std::mutex> lock(g_sysclk_mu);
  initialize_clocks();
  return *g_sysclk_mono;
}

void set_system_wallclock(Clock clock) {
  std::unique_lock<std::mutex> lock(g_sysclk_mu);
  initialize_clocks();
  *g_sysclk_wall = std::move(clock);
}

void set_system_monotonic_clock(Clock clock) {
  std::unique_lock<std::mutex> lock(g_sysclk_mu);
  initialize_clocks();
  *g_sysclk_mono = std::move(clock);
}

}  // namespace base
