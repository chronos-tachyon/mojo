// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/stopwatch.h"

#include <stdexcept>

#include "base/duration.h"

namespace base {

std::pair<Duration, Duration> Stopwatch::durations() const {
  Time end = stop_;
  if (running_)
    end = clock_.now();
  Duration d = end - start_;
  return std::make_pair(d, cumulative_ + d);
}

void Stopwatch::start() {
  assert_stopped();
  Time now = clock_.now();
  cumulative_ += (stop_ - start_);
  start_ = now;
  running_ = true;
}

void Stopwatch::stop() {
  assert_running();
  stop_ = clock_.now();
  running_ = false;
}

void Stopwatch::reset() {
  start_ = Time();
  stop_ = Time();
  cumulative_ = Duration();
  running_ = false;
}

}  // namespace base
