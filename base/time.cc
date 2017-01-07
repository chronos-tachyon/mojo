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
  CHECK_NOTNULL(tv);
  if (tv->tv_sec < 0 || tv->tv_usec < 0) return Result::not_implemented();
  auto d = Duration::raw(false, tv->tv_sec, tv->tv_usec * 1000);
  *out = Time::from_epoch(d);
  return Result();
}

Result time_from_timespec(Time* out, const struct timespec* ts) {
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(ts);
  if (ts->tv_sec < 0 || ts->tv_nsec < 0) return Result::not_implemented();
  auto d = Duration::raw(false, ts->tv_sec, ts->tv_nsec);
  *out = Time::from_epoch(d);
  return Result();
}

Result timeval_from_time(struct timeval* out, Time time) {
  CHECK_NOTNULL(out);
  ::bzero(out, sizeof(*out));
  if (time.before_epoch()) return Result::not_implemented();
  uint64_t s;
  uint32_t ns;
  std::tie(std::ignore, s, ns) = time.since_epoch().raw();
  out->tv_sec = s;
  out->tv_usec = ns / 1000;
  return Result();
}

Result timespec_from_time(struct timespec* out, Time time) {
  CHECK_NOTNULL(out);
  ::bzero(out, sizeof(*out));
  if (time.before_epoch()) return Result::not_implemented();
  uint64_t s;
  uint32_t ns;
  std::tie(std::ignore, s, ns) = time.since_epoch().raw();
  out->tv_sec = s;
  out->tv_nsec = ns;
  return Result();
}

}  // namespace base
