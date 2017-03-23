// base/time/zone.h - Types for representing time zones
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_TIME_ZONE_H
#define BASE_TIME_ZONE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/result.h"
#include "base/strings.h"
#include "base/time/time.h"

namespace base {
namespace time {
namespace zone {

// Type represents a time zone rule.
class Type {
 public:
  Type(StringPiece abbr, int32_t gmtoff, bool dst, bool spec)
      : abbr_(abbr), gmtoff_(gmtoff), dst_(dst), spec_(spec) {}

  Type() : Type("???", 0, false, false) {}

  Type(const Type&) = default;
  Type(Type&&) noexcept = default;
  Type& operator=(const Type&) = default;
  Type& operator=(Type&&) noexcept = default;

  // Returns the abbreviated name for this rule.
  StringPiece abbreviation() const noexcept { return abbr_; }

  // Returns this rule's offset from UTC (positive means east).
  int32_t utc_offset() const noexcept { return gmtoff_; }

  // Returns true iff this rule is considered a "Daylight Saving Time" or
  // "Summer Time" rule.
  bool is_dst() const noexcept { return dst_; }

  // Returns true iff this rule is valid.
  bool is_specified() const noexcept { return spec_; }

  friend bool operator==(const Type& a, const Type& b) noexcept {
    return a.spec_ == b.spec_ && a.dst_ == b.dst_ && a.gmtoff_ == b.gmtoff_ &&
           a.abbr_ == b.abbr_;
  }

  friend bool operator!=(const Type& a, const Type& b) noexcept {
    return !(a == b);
  }

 private:
  std::string abbr_;
  int32_t gmtoff_;
  bool dst_;
  bool spec_;
};

// Recurrence represents a recurring annual event.
class Recurrence {
 public:
  // Mode selects how to interpret a Recurrence.
  enum class Mode : uint8_t {
    // The event never happens.
    never,

    // The event is always happening.
    always,

    // The event happens on a Julian date. (0-based, leap days count)
    //    0 ≤ d ≤ 365
    julian0,

    // The event happens on a Julian date. (1-based, leap days don't count)
    //    1 ≤ d ≤ 365
    julian1,

    // The event happens on Dday of the Wth week of the Mth month.
    //    1=Jan ≤ m ≤ 12=Dec
    //    1=1st ≤ w ≤ 5=last
    //    0=Sun ≤ d ≤ 6=Sat
    month_week_wday,
  };

  Recurrence(Mode mode, uint8_t m, uint8_t w, uint16_t d, int32_t spm) noexcept
      : mode_(mode),
        m_(m),
        w_(w),
        d_(d),
        spm_(spm) {}

  Recurrence() noexcept : Recurrence(Mode::never, 0, 0, 0, 0) {}

  Recurrence(const Recurrence&) noexcept = default;
  Recurrence(Recurrence&&) noexcept = default;
  Recurrence& operator=(const Recurrence&) noexcept = default;
  Recurrence& operator=(Recurrence&&) noexcept = default;

  // Returns the mode of this Recurrence.
  Mode mode() const noexcept { return mode_; }

  // Returns the 1-based month number of this Recurrence.
  // - Only meaningful for |Mode::month_week_wday|
  uint16_t month() const noexcept { return m_; }

  // Returns the 1-based week number of this Recurrence.
  // - Only meaningful for |Mode::month_week_wday|
  uint16_t week() const noexcept { return w_; }

  // Returns the day number of this Recurrence.
  // - Only meaningful for certain values of |mode()|
  // - Interpretation varies with |mode()|
  uint16_t day() const noexcept { return d_; }

  // The number of seconds past midnight at which the event begins.
  // - "Midnight" is 00:00 in the local time zone
  // - This value can be as large as 7 * 24 * 60 * 60, indicating that the
  //   actual event begins one week *after* the indicated day
  // - This value can be as small as -7 * 24 * 60 * 60, indicating that the
  //   actual event begins one week *before* the indicated day
  int32_t seconds_past_midnight() const noexcept { return spm_; }

  friend bool operator==(Recurrence a, Recurrence b) noexcept {
    return a.mode_ == b.mode_ && a.m_ == b.m_ && a.w_ == b.w_ && a.d_ == b.d_ &&
           a.spm_ == b.spm_;
  }

  friend bool operator!=(Recurrence a, Recurrence b) noexcept {
    return !(a == b);
  }

 private:
  Mode mode_;
  uint8_t m_;
  uint8_t w_;
  uint16_t d_;
  int32_t spm_;
};

// Regime represents a pair of rules and the logic for switching between them.
class Regime {
 public:
  Regime(Time t0, Time t1, Recurrence r0, Recurrence r1, const Type* st,
         const Type* dt) noexcept : t0_(t0),
                                    t1_(t1),
                                    r0_(r0),
                                    r1_(r1),
                                    std_(st),
                                    dst_(dt) {}

  Regime() = default;

  Regime(const Regime&) = default;
  Regime(Regime&&) noexcept = default;
  Regime& operator=(const Regime&) = default;
  Regime& operator=(Regime&&) noexcept = default;

  // Returns the earliest time which lies within this Regime.
  Time regime_begin() const noexcept { return t0_; }

  // Returns the earliest time which lies after the end of this Regime.
  Time regime_end() const noexcept { return t1_; }

  // Returns the recurrence for the start of DST/ST.
  // - Mode::never iff this time zone never observes DST/ST
  // - Mode::always iff this time zone is permanently in DST/ST
  Recurrence dst_begin() const noexcept { return r0_; }

  // Returns the recurrence for the end of DST/ST.
  // - Mode::never iff this time zone is permanently in DST/ST
  // - Mode::always iff this time zone never observes DST/ST
  Recurrence dst_end() const noexcept { return r1_; }

  // Returns the rule which is in effect outside of DST/ST.
  const Type* standard_time() const noexcept { return std_; }

  // Returns the rule which is in effect within DST/ST.
  const Type* daylight_time() const noexcept { return dst_; }

  friend bool operator==(Regime a, Regime b) noexcept {
    return a.t0_ == b.t0_ && a.t1_ == b.t1_ && a.r0_ == b.r0_ &&
           a.r1_ == b.r1_ && *a.std_ == *b.std_ && *a.dst_ == *b.dst_;
  }

  friend bool operator!=(Regime a, Regime b) noexcept { return !(a == b); }

 private:
  Time t0_;
  Time t1_;
  Recurrence r0_;
  Recurrence r1_;
  const Type* std_;
  const Type* dst_;
};

class LeapSecond {
 public:
  LeapSecond(Time time, int32_t delta) noexcept : time_(time), delta_(delta) {}

  LeapSecond(const LeapSecond&) noexcept = default;
  LeapSecond(LeapSecond&&) noexcept = default;
  LeapSecond& operator=(const LeapSecond&) noexcept = default;
  LeapSecond& operator=(LeapSecond&&) noexcept = default;

  Time time() const noexcept { return time_; }
  int32_t delta() const noexcept { return delta_; }

  friend bool operator==(LeapSecond a, LeapSecond b) noexcept {
    return a.time_ == b.time_ && a.delta_ == b.delta_;
  }

  friend bool operator!=(LeapSecond a, LeapSecond b) noexcept {
    return !(a == b);
  }

 private:
  Time time_;
  int32_t delta_;
};

class Zone {
 public:
  Zone() = default;
  Zone(const Zone&) = default;
  Zone(Zone&&) noexcept = default;
  Zone& operator=(const Zone&) = default;
  Zone& operator=(Zone&&) noexcept = default;

  StringPiece name() const noexcept { return name_; }
  void set_name(StringPiece name) { name_ = name; }

  const std::vector<Type>& types() const noexcept { return types_; }
  std::vector<Type>& types() noexcept { return types_; }

  const std::vector<Regime>& regimes() const noexcept { return regimes_; }
  std::vector<Regime>& regimes() noexcept { return regimes_; }

  const std::vector<LeapSecond>& leap_seconds() const noexcept {
    return leaps_;
  }
  std::vector<LeapSecond>& leap_seconds() noexcept { return leaps_; }

  const Regime* get_regime(Time time) const;

 private:
  std::string name_;
  std::vector<Type> types_;
  std::vector<Regime> regimes_;
  std::vector<LeapSecond> leaps_;
};

using Pointer = std::shared_ptr<const Zone>;

class Database {
 protected:
  Database() noexcept = default;

 public:
  virtual ~Database() noexcept = default;
  virtual Result get(Pointer* out, StringPiece id) const = 0;
  virtual Result all(std::vector<std::string>* out) const = 0;

  Database(const Database&) = delete;
  Database(Database&&) = delete;
  Database& operator=(const Database&) = delete;
  Database& operator=(Database&&) = delete;
};

using DatabasePointer = std::shared_ptr<const Database>;

class Loader {
 protected:
  Loader() noexcept = default;

 public:
  virtual ~Loader() noexcept = default;
  virtual Result load(std::string* out, StringPiece filename) const = 0;
  virtual Result scan(std::vector<std::string>* out) const = 0;

  Loader(const Loader&) = delete;
  Loader(Loader&&) = delete;
  Loader& operator=(const Loader&) = delete;
  Loader& operator=(Loader&&) = delete;
};

const Pointer& utc();
const Pointer& unknown();

DatabasePointer new_builtin_database();
DatabasePointer new_posix_database();
DatabasePointer new_zoneinfo_database();
DatabasePointer new_zoneinfo_database(StringPiece tzdir);
DatabasePointer new_zoneinfo_database(std::unique_ptr<Loader> loader);
DatabasePointer new_cached_database(DatabasePointer ptr);
DatabasePointer new_meta_database(std::vector<DatabasePointer> vec);

DatabasePointer system_database();
void set_system_database(DatabasePointer tzdb);

// Takes an offset, in seconds east of UTC, and formats it in ISO 8601 format.
//
// Examples:
//      0   => "Z"      (use_zulu=true)
//      0   => "+00:00" (use_zulu=false)
//   3600   => "+01:00"
//  -7200   => "-02:00"
//  45296   => "+12:34:56"
//
std::string format_offset(int32_t offset, bool use_zulu);

}  // namespace zone
}  // namespace time
}  // namespace base

#endif  // BASE_TIME_ZONE_H
