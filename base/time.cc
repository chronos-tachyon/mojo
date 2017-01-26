// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time.h"

#include <time.h>

#include <cstring>
#include <sstream>
#include <stdexcept>

#include "base/logging.h"

namespace base {

void Time::append_to(std::string* out) const {
  std::ostringstream os;
  os << "Time(" << d_.as_string() << ")";  // TODO: friendlier output
  out->append(os.str());
}

std::string Time::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

void MonotonicTime::append_to(std::string* out) const {
  std::ostringstream os;
  os << "MonotonicTime(" << d_.as_string() << ")";
  out->append(os.str());
}

std::string MonotonicTime::as_string() const {
  std::string out;
  append_to(&out);
  return out;
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

}  // namespace base
