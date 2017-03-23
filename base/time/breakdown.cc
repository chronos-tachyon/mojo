// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/breakdown.h"

#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>

#include "base/logging.h"
#include "base/safemath.h"
#include "base/time/duration.h"
#include "base/time/zone.h"

using namespace base::time::internal;

// Number of days in each month
static constexpr int MDAY_BY_MONTH[2][12] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};

// Number of days in the year before the start of each month
static constexpr int YDAY_BY_MONTH[2][13] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366},
};

static constexpr base::StringPiece WDAY_NAMES[2][7] = {
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},
    {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
     "Saturday"},
};

static constexpr base::StringPiece MONTH_NAMES[2][12] = {
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
     "Nov", "Dec"},
    {"January", "February", "March", "April", "May", "June", "July", "August",
     "September", "October", "November", "December"},
};

static constexpr unsigned int is_leapyear(int64_t year) {
  return ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0)) ? 1 : 0;
}

static base::safe<int64_t> convert_y2d(base::safe<int64_t> years) {
  auto a = ((years - 1) / 400);
  years -= a * 400;
  auto b = ((years - 1) / 100);
  years -= b * 100;
  auto c = ((years - 1) / 4);
  years -= c * 4;
  auto d = years;
  return a * DAY_PER_400YEAR + b * DAY_PER_100YEAR + c * DAY_PER_4YEAR +
         d * DAY_PER_YEAR;
}

static base::safe<int64_t> convert_d2y(base::safe<int64_t> days) {
  auto a = (days / DAY_PER_400YEAR);
  days %= DAY_PER_400YEAR;
  auto b = (days / DAY_PER_100YEAR);
  days %= DAY_PER_100YEAR;
  auto c = (days / DAY_PER_4YEAR);
  days %= DAY_PER_4YEAR;
  auto d = (days / DAY_PER_YEAR);
  return a * 400 + b * 100 + c * 4 + d;
}

static uint16_t make_yday(int64_t year, uint8_t month, uint8_t mday) noexcept {
  auto leap = is_leapyear(year);
  DCHECK_GE(month, 0);
  DCHECK_LT(month, 12);
  DCHECK_GE(mday, 0);
  DCHECK_LT(mday, MDAY_BY_MONTH[leap][month]);
  return YDAY_BY_MONTH[leap][month] + mday;
}

static std::pair<uint8_t, uint8_t> make_month_and_mday(int64_t year,
                                                       uint16_t yday) noexcept {
  auto leap = is_leapyear(year);
  CHECK_GE(yday, 0);
  CHECK_LT(yday, 365 + leap);
  uint8_t month = 0;
  while (yday >= YDAY_BY_MONTH[leap][month + 1]) ++month;
  uint8_t mday = yday - YDAY_BY_MONTH[leap][month];
  return std::make_pair(month, mday);
}

static uint8_t make_wday(int64_t year, uint16_t yday) noexcept {
  // https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week#Gauss.27s_algorithm
  auto a = (year - 1) % 400;
  auto b = (year - 1) % 100;
  auto c = (year - 1) % 4;
  return (a * 6 + b * 4 + c * 5 + yday + 1) % 7;
}

static uint32_t make_spm(uint32_t hour, uint32_t minute,
                         uint32_t second) noexcept {
  return hour * SEC_PER_HOUR + minute * SEC_PER_MIN + second;
}

static base::StringPiece month_name(int month, unsigned int want_long) {
  --month;
  if (month < 0 || month >= 12) {
    month %= 12;
    if (month < 0) month += 12;
  }
  return MONTH_NAMES[want_long][month];
}

static base::StringPiece weekday_name(int weekday, unsigned int want_long) {
  if (weekday < 0 || weekday >= 7) {
    weekday %= 7;
    if (weekday < 0) weekday += 7;
  }
  return WDAY_NAMES[want_long][weekday];
}

namespace base {
namespace time {

static RawBreakdown make_raw(Time time, int32_t utc_offset) {
  auto dur = time.since_epoch().raw();
  safe<int64_t> second = dur.s;
  safe<int32_t> nanosecond = dur.ns;
  if (dur.neg) {
    second = -second;
    nanosecond = -nanosecond;
  }

  second += utc_offset;
  if (nanosecond < 0) {
    --second;
    nanosecond += NANO_PER_SEC;
  }

  safe<int64_t> days = (second / SEC_PER_DAY);
  second -= days * SEC_PER_DAY;
  if (second < 0) {
    --days;
    second += SEC_PER_DAY;
  }
  days += Y1970;

  // Make guess, then correct.
  auto years = convert_d2y(days);

  auto actual = convert_y2d(years);
  while (actual > days) {
    --years;
    actual = convert_y2d(years);
  }
  days -= actual;

  // Above code can overcorrect by 1.  Compensate.
  auto dpy = 365 + is_leapyear(years.value());
  if (days >= dpy) {
    ++years;
    days -= dpy;
  }

  auto hour = (second / SEC_PER_HOUR);
  second %= SEC_PER_HOUR;
  auto minute = (second / SEC_PER_MIN);
  second %= SEC_PER_MIN;

  RawBreakdown raw;
  raw.year = years.value();
  raw.yday = days.value<uint16_t>();
  std::tie(raw.month, raw.mday) = make_month_and_mday(raw.year, raw.yday);
  raw.wday = make_wday(raw.year, raw.yday);
  raw.hour = hour.value<uint8_t>();
  raw.minute = minute.value<uint8_t>();
  raw.second = second.value<uint8_t>();
  raw.nanosecond = nanosecond.value<uint32_t>();
  return raw;
}

static uint16_t make_recurrence_yday(int64_t year, zone::Recurrence rec) {
  auto leap = is_leapyear(year);
  uint16_t x;
  using Mode = zone::Recurrence::Mode;
  switch (rec.mode()) {
    case Mode::never:
      return 400;

    case Mode::always:
      return 0;

    case Mode::julian0:
      return rec.day() + 1;

    case Mode::julian1:
      x = rec.day();
      if (leap && x >= 60) ++x;
      return x;

    case Mode::month_week_wday:
      x = YDAY_BY_MONTH[leap][rec.month() - 1];
      while (make_wday(year, x) != rec.day()) ++x;
      for (uint8_t i = rec.week(); i > 1; --i) {
        x += 7;
      }
      while (x >= YDAY_BY_MONTH[leap][rec.month()]) x -= 7;
      ++x;
      return x;
  }
  LOG(DFATAL) << "Unknown base::time::zone::Recurrence::Mode: "
              << static_cast<uint16_t>(rec.mode());
  return 999;
}

static std::pair<uint16_t, uint32_t> make_recurrence(int64_t year,
                                                     zone::Recurrence rec) {
  auto yday = make_recurrence_yday(year, rec);
  auto spm = rec.seconds_past_midnight();
  while (spm < 0) {
    --yday;
    spm += SEC_PER_DAY;
  }
  while (spm >= SEC_PER_DAY) {
    ++yday;
    spm -= SEC_PER_DAY;
  }
  return std::make_pair(yday, spm);
}

Breakdown::Breakdown(Time time) {
  raw_ = make_raw(time, 0);
  hint_ = Hint::standard_time;
  zone_ = zone::utc();
  type_ = &zone_->types()[0];
}

Breakdown::Breakdown(Time time, zone::Pointer zone) {
  const zone::Regime* regime = CHECK_NOTNULL(zone->get_regime(time));
  const zone::Type* st = CHECK_NOTNULL(regime->standard_time());
  const zone::Type* dt = CHECK_NOTNULL(regime->daylight_time());

  auto raw0 = make_raw(time, st->utc_offset());
  auto raw1 = make_raw(time, dt->utc_offset());

  uint16_t dt_yday, st_yday;
  uint32_t dt_spm, st_spm;
  std::tie(dt_yday, dt_spm) = make_recurrence(raw0.year, regime->dst_begin());
  std::tie(st_yday, st_spm) = make_recurrence(raw1.year, regime->dst_end());

  auto spm0 = make_spm(raw0.hour, raw0.minute, raw0.second);
  auto spm1 = make_spm(raw1.hour, raw1.minute, raw1.second);

  auto yday0 = raw0.yday + 1;
  auto yday1 = raw1.yday + 1;

  bool dst;
  if (dt_yday < st_yday) {
    if (yday0 < dt_yday)
      dst = false;
    else if (yday0 == dt_yday && spm0 < dt_spm)
      dst = false;
    else if (yday1 < st_yday)
      dst = true;
    else if (yday1 == st_yday && spm1 < st_spm)
      dst = true;
    else
      dst = false;
  } else {
    if (yday1 < st_yday)
      dst = true;
    else if (yday1 == st_yday && spm1 < st_spm)
      dst = true;
    else if (yday0 < dt_yday)
      dst = false;
    else if (yday0 == dt_yday && spm0 < dt_spm)
      dst = false;
    else
      dst = true;
  }

  if (dst) {
    raw_ = raw1;
    zone_ = std::move(zone);
    type_ = dt;
    hint_ = Hint::daylight_saving_time;
  } else {
    raw_ = raw0;
    zone_ = std::move(zone);
    type_ = st;
    hint_ = Hint::standard_time;
  }
}

Breakdown::Breakdown(int64_t year, uint8_t month, uint8_t mday, uint8_t hour,
                     uint8_t minute, uint8_t second, uint32_t nano) {
  auto leap = is_leapyear(year);

  if (month < 1 || month > 12) throw std::overflow_error("month out of range");
  if (mday < 1 || mday > MDAY_BY_MONTH[leap][month - 1])
    throw std::overflow_error("mday out of range");
  if (hour > 23) throw std::overflow_error("hour out of range");
  if (minute > 59) throw std::overflow_error("minute out of range");
  if (second > 59) throw std::overflow_error("second out of range");
  if (nano > 999999999) throw std::overflow_error("nanosecond out of range");

  --month;
  --mday;
  auto yday = make_yday(year, month, mday);
  auto wday = make_wday(year, yday);
  raw_ =
      RawBreakdown(year, yday, month, mday, wday, hour, minute, second, nano);
  hint_ = Hint::standard_time;
  zone_ = zone::utc();
  type_ = &zone_->types()[0];
}

Breakdown::Breakdown(int64_t year, uint8_t month, uint8_t mday, uint8_t hour,
                     uint8_t minute, uint8_t second, uint32_t nano,
                     zone::Pointer tz) {
  auto leap = is_leapyear(year);

  if (month < 1 || month > 12) throw std::overflow_error("month out of range");
  if (mday < 1 || mday > MDAY_BY_MONTH[leap][month - 1])
    throw std::overflow_error("mday out of range");
  if (hour > 23) throw std::overflow_error("hour out of range");
  if (minute > 59) throw std::overflow_error("minute out of range");
  if (second > 59) throw std::overflow_error("second out of range");
  if (nano > 999999999) throw std::overflow_error("nanosecond out of range");

  --month;
  --mday;
  auto yday = make_yday(year, month, mday);
  auto wday = make_wday(year, yday);
  raw_ =
      RawBreakdown(year, yday, month, mday, wday, hour, minute, second, nano);
  hint_ = Hint::standard_time;
  zone_ = std::move(tz);
  type_ = &zone_->types()[0];
}

std::string Breakdown::iso8601() const {
  std::ostringstream o;
  o << std::setfill('0');
  o << std::setw(4) << year();
  o << '-' << std::setw(2) << uint16_t(month());
  o << '-' << std::setw(2) << uint16_t(mday());
  o << 'T' << std::setw(2) << uint16_t(hour());
  o << ':' << std::setw(2) << uint16_t(minute());
  o << ':' << std::setw(2) << uint16_t(second());
  o << '.' << std::setw(9) << nanosecond();
  if (type_ && type_->is_specified())
    o << zone::format_offset(type_->utc_offset(), true);
  return o.str();
}

std::string Breakdown::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

void Breakdown::append_to(std::string* out) const {
  CHECK_NOTNULL(out);
  std::ostringstream o;
  o << std::setfill('0');
  o << "{";
  o << std::setw(4) << year();
  o << "," << std::setw(2) << uint16_t(month());
  o << "," << std::setw(2) << uint16_t(mday());
  o << "," << std::setw(2) << uint16_t(hour());
  o << "," << std::setw(2) << uint16_t(minute());
  o << "," << std::setw(2) << uint16_t(second());
  o << "," << std::setw(9) << nanosecond();
  o << ",wday=" << weekday_short_name(wday());
  o << ",yday=" << std::setw(3) << yday();
  if (zone_) o << ",tz=" << zone_->name();
  o << "}";
  out->append(o.str());
}

std::size_t Breakdown::length_hint() const noexcept { return 63; }

bool operator==(const Breakdown& a, const Breakdown& b) noexcept {
  return a.year() == b.year() && a.month() == b.month() &&
         a.mday() == b.mday() && a.hour() == b.hour() &&
         a.minute() == b.minute() && a.second() == b.second() &&
         a.nanosecond() == b.nanosecond() && a.timezone() == b.timezone() &&
         a.timezone_type() == b.timezone_type() &&
         a.timezone_hint() == b.timezone_hint();
}

bool operator<(const Breakdown& a, const Breakdown& b) noexcept {
  if (a.year() != b.year()) return a.year() < b.year();
  if (a.month() != b.month()) return a.month() < b.month();
  if (a.mday() != b.mday()) return a.mday() < b.mday();
  if (a.hour() != b.hour()) return a.hour() < b.hour();
  if (a.minute() != b.minute()) return a.minute() < b.minute();
  if (a.second() != b.second()) return a.second() < b.second();
  return a.nanosecond() < b.nanosecond();
}

std::ostream& operator<<(std::ostream& o, const Breakdown& breakdown) {
  return (o << breakdown.as_string());
}

StringPiece month_short_name(int month) { return month_name(month, 0); }

StringPiece month_long_name(int month) { return month_name(month, 1); }

StringPiece weekday_short_name(int weekday) { return weekday_name(weekday, 0); }

StringPiece weekday_long_name(int weekday) { return weekday_name(weekday, 1); }

}  // namespace time
}  // namespace base
