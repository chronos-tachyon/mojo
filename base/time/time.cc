// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/time.h"

#include <time.h>

#include <cstring>
#include <ostream>
#include <stdexcept>

#include "base/logging.h"
#include "base/time/breakdown.h"

namespace base {
namespace time {

using namespace internal;

static constexpr uint64_t X = std::numeric_limits<int64_t>::max();
static const Time MIN = Time(Duration(DurationRep(true, X, NANO_PER_SEC - 1)));
static const Time MAX = Time(Duration(DurationRep(false, X, NANO_PER_SEC - 1)));

void Time::append_to(std::string* out) const {
  CHECK_NOTNULL(out);
  if (*this < MIN) {
    out->append("[infinite past]");
  } else if (*this > MAX) {
    out->append("[infinite future]");
  } else {
    Breakdown b;
    b.set(*this);
    out->append(b.iso8601());
  }
}

std::size_t Time::length_hint() const noexcept { return 30; }

std::string Time::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

std::ostream& operator<<(std::ostream& o, Time t) {
  return (o << t.as_string());
}

void MonotonicTime::append_to(std::string* out) const {
  CHECK_NOTNULL(out);
  out->push_back('M');
  d_.append_to(out);
}

std::size_t MonotonicTime::length_hint() const noexcept {
  return d_.length_hint() + 1;
}

std::string MonotonicTime::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

std::ostream& operator<<(std::ostream& o, MonotonicTime t) {
  return (o << t.as_string());
}

Result time_from_timeval(Time* out, const struct timeval* tv) {
  CHECK_NOTNULL(out);
  Duration d;
  auto r = duration_from_timeval(&d, tv);
  if (r) *out = Time::from_epoch(d);
  return r;
}

Result time_from_timespec(Time* out, const struct timespec* ts) {
  CHECK_NOTNULL(out);
  Duration d;
  auto r = duration_from_timespec(&d, ts);
  if (r) *out = Time::from_epoch(d);
  return r;
}

Result timeval_from_time(struct timeval* out, Time time) {
  return timeval_from_duration(out, time.since_epoch());
}

Result timespec_from_time(struct timespec* out, Time time) {
  return timespec_from_duration(out, time.since_epoch());
}

}  // namespace time
}  // namespace base
