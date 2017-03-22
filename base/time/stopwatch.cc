// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/stopwatch.h"

#include <stdexcept>

#include "base/logging.h"

namespace base {
namespace time {

void Stopwatch::Measurement::assert_valid() const {
  if (!ptr_) {
    LOG(FATAL) << "BUG: base::time::Stopwatch::Measurement is empty!";
  }
}

void Stopwatch::assert_stopped() const {
  if (running_) {
    LOG(DFATAL) << "BUG: base::time::Stopwatch is running!";
  }
}

void Stopwatch::assert_running() const {
  if (!running_) {
    LOG(DFATAL) << "BUG: base::time::Stopwatch is not running!";
  }
}

std::pair<Duration, Duration> Stopwatch::durations() const {
  MonotonicTime end = stop_;
  if (running_) end = clock_.now();
  Duration d = end - start_;
  return std::make_pair(d, cumulative_ + d);
}

void Stopwatch::start() {
  assert_stopped();
  MonotonicTime now = clock_.now();
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
  start_ = MonotonicTime();
  stop_ = MonotonicTime();
  cumulative_ = Duration();
  running_ = false;
}

}  // namespace time
}  // namespace base
