// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/duration.h"

#include <cmath>
#include <ostream>

#include "base/int128.h"
#include "base/logging.h"

using namespace base::internal;

namespace base {

const Duration NANOSECOND = Duration::from_raw(false, 0U, 1U);
const Duration MICROSECOND = Duration::from_raw(false, 0U, NS_PER_US);
const Duration MILLISECOND = Duration::from_raw(false, 0U, NS_PER_MS);
const Duration SECOND = Duration::from_raw(false, 1U, 0U);
const Duration MINUTE = Duration::from_raw(false, S_PER_MIN, 0U);
const Duration HOUR = Duration::from_raw(false, S_PER_HR, 0U);

using U64 = safe<uint64_t>;
using U32 = safe<uint32_t>;
using S64 = safe<int64_t>;
using LD = long double;

static UInt128 to_u128(Duration d) {
  auto rep = d.raw();
  return UInt128(rep.s.value()) * NS_PER_S + UInt128(rep.ns.value());
}

static Duration from_u128(bool neg, UInt128 x) {
  auto pair = divmod(x, NS_PER_S);
  auto s = U64(uint64_t(pair.first));
  auto ns = U32(uint32_t(pair.second));
  return Duration::from_raw(neg, s, ns);
}

static LD to_ldbl(Duration d) {
  auto rep = d.raw();
  auto x = LD(rep.s);
  auto y = LD(NS_PER_S);
  auto z = LD(rep.ns);
  auto v = ::fma(x, y, z);
  if (rep.neg) v = -v;
  return v;
}

static Duration from_ldbl(LD x) {
  bool neg = false;
  if (x < 0) {
    neg = true;
    x = -x;
  }
  x = ::round(x);
  auto quo = ::floor(x / NS_PER_S);
  auto rem = ::floor(x - quo * NS_PER_S);
  return Duration::from_raw(neg, quo, rem);
}

Duration operator*(Duration a, safe<uint64_t> b) {
  return Duration::from_raw(a.rep_.neg, a.rep_.s * b, U64(a.rep_.ns) * b);
}

Duration operator*(Duration a, safe<int64_t> b) {
  if (b < 0)
    return -a * (U64(-(b + 1)) + 1);
  else
    return a * U64(b);
}

Duration operator*(Duration a, double b) { return from_ldbl(to_ldbl(a) * b); }

Duration operator/(Duration a, safe<uint64_t> b) {
  return from_u128(a.is_neg(), to_u128(a) / UInt128(b.value()));
}

Duration operator/(Duration a, safe<int64_t> b) {
  if (b < 0)
    return (-a) / (U64(-(b + 1)) + 1);
  else
    return a / U64(b);
}

Duration operator/(Duration a, double b) {
  if (b == 0) throw std::domain_error("divide by zero");
  return from_ldbl(to_ldbl(a) / b);
}

double operator/(Duration a, Duration b) {
  if (b.is_zero()) throw std::domain_error("divide by zero");
  return to_ldbl(a) / to_ldbl(b);
}

std::pair<double, Duration> divmod(Duration a, Duration b) {
  if (b.is_zero()) throw std::domain_error("divide by zero");
  auto p = to_ldbl(a);
  auto q = to_ldbl(b);
  auto quo = ::floor(p / q);
  auto rem = ::floor(p - quo * q);
  return std::make_pair(quo, from_ldbl(rem));
}

void Duration::append_to(std::string* out) const {
  CHECK_NOTNULL(out);
  uint64_t s = rep_.s.value();
  uint32_t ns = rep_.ns.value();
  if (is_neg()) out->push_back('-');
  if (s > 0) {
    if (s >= S_PER_HR) {
      auto hr = s / S_PER_HR;
      s = s % S_PER_HR;
      ull_append_to(out, hr);
      out->push_back('h');
    }
    if (s >= S_PER_MIN) {
      auto min = s / S_PER_MIN;
      s = s % S_PER_MIN;
      ui_append_to(out, min);
      out->push_back('m');
    }
    if (s || ns) {
      ui_append_to(out, s);
      if (ns > 0) {
        char reversed[9];
        for (std::size_t i = 0; i < 9; ++i) {
          reversed[i] = "0123456789"[ns % 10];
          ns /= 10;
        }

        std::size_t j = 0;
        while (j < 9 && reversed[j] == '0') ++j;

        std::size_t i = 9;
        out->push_back('.');
        while (i > j) {
          --i;
          out->push_back(reversed[i]);
        }
      }
      out->push_back('s');
    }
  } else if (ns >= NS_PER_MS && (ns % NS_PER_MS) == 0) {
    ui_append_to(out, ns / NS_PER_MS);
    out->append("ms");
  } else if (ns >= NS_PER_US && (ns % NS_PER_US) == 0) {
    ui_append_to(out, ns / NS_PER_US);
    out->append("µs");
  } else if (ns > 0) {
    ui_append_to(out, ns);
    out->append("ns");
  } else {
    out->push_back('0');
  }
}

std::size_t Duration::length_hint() const noexcept {
  return ::ceil(std::log10(rep_.s.value())) + 12;
}

std::string Duration::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

std::ostream& operator<<(std::ostream& o, Duration d) {
  return (o << d.as_string());
}

Result duration_from_timeval(Duration* out, const struct timeval* tv) {
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(tv);
  if (tv->tv_sec < 0 || tv->tv_usec < 0) return Result::not_implemented();
  *out = Duration::from_raw(false, tv->tv_sec, tv->tv_usec * 1000);
  return Result();
}

Result duration_from_timespec(Duration* out, const struct timespec* ts) {
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(ts);
  if (ts->tv_sec < 0 || ts->tv_nsec < 0) return Result::not_implemented();
  *out = Duration::from_raw(false, ts->tv_sec, ts->tv_nsec);
  return Result();
}

Result timeval_from_duration(struct timeval* out, Duration dur) {
  CHECK_NOTNULL(out);
  ::bzero(out, sizeof(*out));
  if (dur.is_neg()) return Result::not_implemented();
  auto rep = dur.raw();
  out->tv_sec = rep.s.value();
  out->tv_usec = rep.ns.value() / 1000;
  return Result();
}

Result timespec_from_duration(struct timespec* out, Duration dur) {
  CHECK_NOTNULL(out);
  ::bzero(out, sizeof(*out));
  if (dur.is_neg()) return Result::not_implemented();
  auto rep = dur.raw();
  out->tv_sec = rep.s.value();
  out->tv_nsec = rep.ns.value();
  return Result();
}

}  // namespace base
