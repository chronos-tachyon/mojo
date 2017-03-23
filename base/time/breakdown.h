// base/time/breakdown.h - Type for breaking down Time into human units
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_TIME_BREAKDOWN_H
#define BASE_TIME_BREAKDOWN_H

#include <cstdint>
#include <iosfwd>
#include <memory>

#include "base/result.h"
#include "base/strings.h"
#include "base/time/time.h"
#include "base/time/zone.h"

namespace base {
namespace time {

constexpr int Jan = 1;
constexpr int Feb = 2;
constexpr int Mar = 3;
constexpr int Apr = 4;
constexpr int May = 5;
constexpr int Jun = 6;
constexpr int Jul = 7;
constexpr int Aug = 8;
constexpr int Sep = 9;
constexpr int Oct = 10;
constexpr int Nov = 11;
constexpr int Dec = 12;

constexpr int Sun = 0;
constexpr int Mon = 1;
constexpr int Tue = 2;
constexpr int Wed = 3;
constexpr int Thu = 4;
constexpr int Fri = 5;
constexpr int Sat = 6;

namespace internal {
struct RawBreakdown {
  int64_t year;         // 1970=1970 CE
  uint16_t yday;        // 0=Jan 1st .. 365=Dec 31 (LY)
  uint8_t month;        // 0=Jan .. 11=Dec
  uint8_t mday;         // 0=1st .. 30=31st
  uint8_t wday;         // 0=Sun .. 6=Sat
  uint8_t hour;         // 0 .. 23
  uint8_t minute;       // 0 .. 59
  uint8_t second;       // 0 .. 59
  uint32_t nanosecond;  // 0 .. 999,999,999

  RawBreakdown(int64_t year, uint16_t yday, uint8_t month, uint8_t mday,
               uint8_t wday, uint8_t hour, uint8_t minute, uint8_t second,
               uint32_t nanosecond) noexcept : year(year),
                                               yday(yday),
                                               month(month),
                                               mday(mday),
                                               wday(wday),
                                               hour(hour),
                                               minute(minute),
                                               second(second),
                                               nanosecond(nanosecond) {}
  RawBreakdown() noexcept : RawBreakdown(1970, 0, 0, 0, 4, 0, 0, 0, 0) {}
  RawBreakdown(const RawBreakdown&) noexcept = default;
  RawBreakdown(RawBreakdown&&) noexcept = default;
  RawBreakdown& operator=(const RawBreakdown&) noexcept = default;
  RawBreakdown& operator=(RawBreakdown&&) noexcept = default;
};
}  // namespace internal

class Breakdown {
 public:
  enum class Hint : int8_t {
    force_standard_time = -2,
    standard_time = -1,
    guess = 0,
    daylight_saving_time = 1,
    force_daylight_saving_time = 2,
  };

  // Internal use only.
  Breakdown(internal::RawBreakdown raw, zone::Pointer zone,
            const zone::Type* type, Hint hint)
      : raw_(raw), hint_(hint), zone_(std::move(zone)), type_(type) {}

  // UTC, from Time.
  Breakdown(Time time);

  // Local TZ, from Time.
  Breakdown(Time time, zone::Pointer tz);

  // UTC, specified to nanosecond.
  Breakdown(int64_t year, uint8_t month, uint8_t mday, uint8_t hour,
            uint8_t minute, uint8_t second, uint32_t nanosecond);

  // UTC, specified to second.
  Breakdown(int64_t year, uint8_t month, uint8_t mday, uint8_t hour,
            uint8_t minute, uint8_t second)
      : Breakdown(year, month, mday, hour, minute, second, 0) {}

  // UTC, specified to day.
  Breakdown(int64_t year, uint8_t month, uint8_t mday)
      : Breakdown(year, month, mday, 0, 0, 0, 0) {}

  // UTC, unspecified.
  Breakdown() : Breakdown(1970, 1, 1) {}

  // Local TZ, specified to nanosecond.
  Breakdown(int64_t year, uint8_t month, uint8_t mday, uint8_t hour,
            uint8_t minute, uint8_t second, uint32_t nanosecond,
            zone::Pointer tz);

  // Local TZ, specified to second.
  Breakdown(int64_t year, uint8_t month, uint8_t mday, uint8_t hour,
            uint8_t minute, uint8_t second, zone::Pointer tz)
      : Breakdown(year, month, mday, hour, minute, second, 0, std::move(tz)) {}

  // Local TZ, specified to day.
  Breakdown(int64_t year, uint8_t month, uint8_t mday, zone::Pointer tz)
      : Breakdown(year, month, mday, 0, 0, 0, 0, std::move(tz)) {}

  Breakdown(const Breakdown&) noexcept = default;
  Breakdown(Breakdown&&) noexcept = default;
  Breakdown& operator=(const Breakdown&) noexcept = default;
  Breakdown& operator=(Breakdown&&) noexcept = default;

  // Returns true iff this object represents a valid date and time.
  bool is_valid() const noexcept { return !!type_; }

  // Returns the year C.E.
  int64_t year() const noexcept { return raw_.year; }

  // Returns the 1-based month: 1=Jan, 2=Feb, etc.
  uint8_t month() const noexcept { return raw_.month + 1; }

  // Returns the 1-based day of month: 1=1st, 2=2nd, etc.
  uint8_t mday() const noexcept { return raw_.mday + 1; }

  // Returns the hour, from 0 to 23.
  uint8_t hour() const noexcept { return raw_.hour; }

  // Returns the minute, from 0 to 59.
  uint8_t minute() const noexcept { return raw_.minute; }

  // Returns the second, from 0 to 59.
  uint8_t second() const noexcept { return raw_.second; }

  // Returns the nanosecond, from 0 to 999,999,999.
  uint32_t nanosecond() const noexcept { return raw_.nanosecond; }

  // Returns the 1-based Julian date.
  uint16_t yday() const noexcept { return raw_.yday + 1; }

  // Returns the day of the week: 0=Sun, 1=Mon, etc.
  uint8_t wday() const noexcept { return raw_.wday; }

  // Returns the timezone.
  const zone::Pointer& timezone() const noexcept { return zone_; }

  // Returns the timezone type for the represented time.
  const zone::Type* timezone_type() const noexcept { return type_; }

  Hint timezone_hint() const noexcept { return hint_; }

  void reset() noexcept { *this = Breakdown(); }

  template <typename First, typename... Rest>
  void set(First&& first, Rest&&... rest) {
    *this = Breakdown(std::forward<First>(first), std::forward<Rest>(rest)...);
  }

  void set_year(int64_t n);
  void set_month(int64_t n);
  void set_mday(int64_t n);
  void set_hour(int64_t n);
  void set_minute(int64_t n);
  void set_second(int64_t n);
  void set_nanosecond(int64_t n);
  void set_timezone(zone::Pointer tz);
  void set_timezone_hint(Hint h);

  void add_years(int64_t n);
  void add_months(int64_t n);
  void add_weeks(int64_t n);
  void add_days(int64_t n);
  void add_hours(int64_t n);
  void add_minutes(int64_t n);
  void add_seconds(int64_t n);
  void add_nanoseconds(int64_t n);

  Time time() const;
  std::string iso8601() const;

  std::string as_string() const;
  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept;

 private:
  internal::RawBreakdown raw_;
  Hint hint_;
  zone::Pointer zone_;
  const zone::Type* type_;
};

bool operator==(const Breakdown& a, const Breakdown& b) noexcept;
bool operator<(const Breakdown& a, const Breakdown& b) noexcept;
inline bool operator!=(const Breakdown& a, const Breakdown& b) noexcept {
  return !(a == b);
}
inline bool operator>(const Breakdown& a, const Breakdown& b) noexcept {
  return (b < a);
}
inline bool operator<=(const Breakdown& a, const Breakdown& b) noexcept {
  return !(b < a);
}
inline bool operator>=(const Breakdown& a, const Breakdown& b) noexcept {
  return !(a < b);
}

std::ostream& operator<<(std::ostream& o, const Breakdown& breakdown);

StringPiece month_short_name(int month);
StringPiece month_long_name(int month);

StringPiece weekday_short_name(int weekday);
StringPiece weekday_long_name(int weekday);

}  // namespace time
}  // namespace base

#endif  // BASE_TIME_BREAKDOWN_H
