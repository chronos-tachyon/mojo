// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/zone_posix.h"

#include "base/logging.h"
#include "external/com_googlesource_code_re2/re2/re2.h"

namespace base {
namespace time {
namespace zone {

using Mode = Recurrence::Mode;

static bool consume_name(re2::StringPiece* out, re2::StringPiece& input,
                         bool required) {
  re2::StringPiece name;
  bool ok = re2::RE2::Consume(&input, "([A-Za-z]+)", &name);
  if (ok) *out = name;
  return ok || !required;
}

static bool consume_offset(int32_t* out, re2::StringPiece& input,
                           bool required) {
  re2::StringPiece sign;
  int32_t hh = 0, mm = 0, ss = 0;
  if (!re2::RE2::Consume(&input, "([+-]?)([0-9]+)", &sign, &hh)) {
    return !required;
  }
  re2::RE2::Consume(&input, ":([0-9]+)", &mm);
  re2::RE2::Consume(&input, ":([0-9]+)", &ss);
  if (hh > 168 || mm >= 60 || ss >= 60) return false;
  if (hh == 168 && (mm != 0 || ss != 0)) return false;
  int32_t x = hh * 3600 + mm * 60 + ss;
  if (sign == "-") x = -x;
  *out = -x;  // NOTE: POSIX is backwards ("positive" means "west")
  return true;
}

static bool consume_recurrence(Recurrence* out, re2::StringPiece& input) {
  Mode mode;
  uint32_t m = 0, w = 0, d = 0;
  if (re2::RE2::Consume(&input, ",M([0-9]+)\\.([0-9]+)\\.([0-9]+)", &m, &w,
                        &d)) {
    if (m < 1 || m > 12 || w < 1 || w > 5 || d < 0 || d > 6) return false;
    mode = Mode::month_week_wday;
  } else if (re2::RE2::Consume(&input, ",J([0-9]+)", &d)) {
    if (d < 1 || d > 365) return false;
    mode = Mode::julian1;
  } else if (re2::RE2::Consume(&input, ",([0-9]+)", &d)) {
    if (d < 0 || d > 365) return false;
    mode = Mode::julian0;
  } else {
    return false;
  }

  re2::StringPiece sign;
  int32_t hh = 0, mm = 0, ss = 0;
  int32_t secs = 7200;
  if (re2::RE2::Consume(&input, "/([+-]?)([0-9]+)", &sign, &hh)) {
    re2::RE2::Consume(&input, ":([0-9]+)", &mm);
    re2::RE2::Consume(&input, ":([0-9]+)", &ss);
    if (hh > 168 || mm >= 60 || ss >= 60) return false;
    if (hh == 168 && (mm != 0 || ss != 0)) return false;
    int32_t x = hh * 3600 + mm * 60 + ss;
    if (sign == "-") x = -x;
    secs = x;
  }

  *out = Recurrence(mode, m, w, d, secs);
  return true;
}

Result parse_posix(PosixRules* out, StringPiece spec) {
  CHECK_NOTNULL(out);
  *out = PosixRules();
  out->spec = spec;

  re2::StringPiece input = spec;

  re2::StringPiece name;
  if (!consume_name(&name, input, true))
    return Result::invalid_argument(
        "invalid name for Standard Time in POSIX timezone spec");

  int32_t gmtoff;
  if (!consume_offset(&gmtoff, input, true))
    return Result::invalid_argument(
        "invalid UTC offset for Standard Time in POSIX timezone spec");

  out->standard_time = Type(name, gmtoff, false, true);

  if (input.empty()) {
    // Fixed offset, no DST change
    out->daylight_time = out->standard_time;
    out->dst_start = Recurrence(Mode::never, 0, 0, 0, 0);
    out->dst_end = Recurrence(Mode::always, 0, 0, 0, 0);
    return Result();
  }

  if (!consume_name(&name, input, true))
    return Result::invalid_argument(
        "invalid name for Summer Time in POSIX timezone spec");

  gmtoff += 3600;
  if (!consume_offset(&gmtoff, input, false))
    return Result::invalid_argument(
        "invalid UTC offset for Summer Time in POSIX timezone spec");

  out->daylight_time = Type(name, gmtoff, true, true);

  if (input.empty()) {
    // DST rules not specified - default to US rules

    // Spring forward on 2nd Sunday of March at 02:00
    out->dst_start = Recurrence(Mode::month_week_wday, 3, 2, 0, 7200);

    // Fall back on 1st Sunday of November at 02:00
    out->dst_end = Recurrence(Mode::month_week_wday, 11, 1, 0, 7200);

    return Result();
  }

  if (!consume_recurrence(&out->dst_start, input))
    return Result::invalid_argument(
        "invalid transition rule for Summer Time in POSIX timezone spec");

  if (!consume_recurrence(&out->dst_end, input))
    return Result::invalid_argument(
        "invalid transition rule for Standard Time in POSIX timezone spec");

  if (!input.empty())
    return Result::invalid_argument(
        "found trailing junk in POSIX timezone spec");

  if (out->dst_start == Recurrence(Mode::julian1, 0, 0, 1, 0) &&
      out->dst_end == Recurrence(Mode::julian1, 0, 0, 365, 90000)) {
    out->dst_start = Recurrence(Mode::always, 0, 0, 0, 0);
    out->dst_end = Recurrence(Mode::never, 0, 0, 0, 0);
  }

  return Result();
}

Pointer interpret_posix(const PosixRules& in) {
  auto out = std::make_shared<Zone>();
  auto& types = out->types();
  auto& regimes = out->regimes();
  out->set_name(in.spec);
  types.push_back(in.standard_time);
  if (in.dst_start.mode() == Mode::never) {
    regimes.emplace_back(Time::min(), Time::max(), in.dst_start, in.dst_end,
                         &types[0], &types[0]);
  } else if (in.dst_start.mode() == Mode::always) {
    types.push_back(in.daylight_time);
    regimes.emplace_back(Time::min(), Time::max(), in.dst_start, in.dst_end,
                         &types[1], &types[1]);
  } else {
    types.push_back(in.daylight_time);
    regimes.emplace_back(Time::min(), Time::max(), in.dst_start, in.dst_end,
                         &types[0], &types[1]);
  }
  return out;
}

}  // namespace zone
}  // namespace time
}  // namespace base
