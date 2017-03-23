// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/result_testing.h"
#include "base/time/breakdown.h"
#include "base/time/duration.h"
#include "base/time/zone.h"
#include "gtest/gtest.h"

using namespace base::time;

static constexpr bool F = false;
static constexpr bool T = true;
static constexpr uint32_t X = 999999999;
static constexpr uint32_t Y = 789000000;

struct Expectation {
  int64_t year;
  uint16_t month;
  uint16_t mday;
  uint16_t hour;
  uint16_t minute;
  uint16_t second;
  uint32_t nanosecond;
  uint16_t wday;
  uint16_t yday;
  zone::Pointer zone;
  bool is_dst;
};

static std::ostream& operator<<(std::ostream& o, const Expectation& x) {
  o << "{year=" << x.year << ", "
    << "month=" << x.month << ", "
    << "mday=" << x.mday << ", "
    << "hour=" << x.hour << ", "
    << "minute=" << x.minute << ", "
    << "second=" << x.second << ", "
    << "nanosecond=" << x.nanosecond << ", "
    << "wday=" << x.wday << ", "
    << "yday=" << x.yday << ", "
    << "tz=" << (x.zone ? x.zone->name() : "<nullptr>") << ", "
    << "is_dst=" << (x.is_dst ? "true" : "false") << "}";
  return o;
}

static testing::AssertionResult Matches(const char* expr0, const char* expr1,
                                        const Expectation& expected,
                                        const Breakdown& actual) {
  auto fail = testing::AssertionFailure();
  fail << "Expected: " << expr0 << "\n"
       << "  Actual: " << expr1 << "\n"
       << "Differ because:";

  bool did_fail = false;
  if (expected.year != actual.year()) {
    did_fail = true;
    fail << "\n  year: " << expected.year << " != " << actual.year();
  }
  if (expected.month != actual.month()) {
    did_fail = true;
    fail << "\n  month: " << expected.month
         << " != " << uint16_t(actual.month());
  }
  if (expected.mday != actual.mday()) {
    did_fail = true;
    fail << "\n  mday: " << expected.mday << " != " << uint16_t(actual.mday());
  }
  if (expected.hour != actual.hour()) {
    did_fail = true;
    fail << "\n  hour: " << expected.hour << " != " << uint16_t(actual.hour());
  }
  if (expected.minute != actual.minute()) {
    did_fail = true;
    fail << "\n  minute: " << expected.minute
         << " != " << uint16_t(actual.minute());
  }
  if (expected.second != actual.second()) {
    did_fail = true;
    fail << "\n  second: " << expected.second
         << " != " << uint16_t(actual.second());
  }
  if (expected.nanosecond != actual.nanosecond()) {
    did_fail = true;
    fail << "\n  nanosecond: " << expected.nanosecond
         << " != " << actual.nanosecond();
  }
  if (expected.wday != actual.wday()) {
    did_fail = true;
    fail << "\n  wday: " << expected.wday << " != " << uint16_t(actual.wday());
  }
  if (expected.yday != actual.yday()) {
    did_fail = true;
    fail << "\n  yday: " << expected.yday << " != " << actual.yday();
  }
  if (expected.zone != actual.timezone()) {
    did_fail = true;
    fail << "\n  zone: " << expected.zone->name()
         << " != " << actual.timezone()->name();
  }
  if (expected.is_dst != actual.timezone_type()->is_dst()) {
    did_fail = true;
    fail << "\n  is_dst: " << expected.is_dst
         << " != " << actual.timezone_type()->is_dst();
  }
  if (did_fail)
    return fail;
  else
    return testing::AssertionSuccess();
}

TEST(Breakdown, SetUTC) {
  struct TestRow {
    internal::DurationRep input;
    Expectation expected;
  };

  const auto& UTC = base::time::zone::utc();
  std::vector<TestRow> testdata = {
      // 1970
      {{F, 0, 0}, {1970, Jan, 1, 0, 0, 0, 0, Thu, 1, UTC, F}},
      {{F, 5097599, X}, {1970, Feb, 28, 23, 59, 59, X, Sat, 59, UTC, F}},
      {{F, 5097600, 0}, {1970, Mar, 1, 0, 0, 0, 0, Sun, 60, UTC, F}},
      {{F, 31535999, X}, {1970, Dec, 31, 23, 59, 59, X, Thu, 365, UTC, F}},

      // 1971
      {{F, 31536000, 0}, {1971, Jan, 1, 0, 0, 0, 0, Fri, 1, UTC, F}},
      {{F, 36633599, X}, {1971, Feb, 28, 23, 59, 59, X, Sun, 59, UTC, F}},
      {{F, 36633600, 0}, {1971, Mar, 1, 0, 0, 0, 0, Mon, 60, UTC, F}},
      {{F, 63071999, X}, {1971, Dec, 31, 23, 59, 59, X, Fri, 365, UTC, F}},

      // 1972
      {{F, 63072000, 0}, {1972, Jan, 1, 0, 0, 0, 0, Sat, 1, UTC, F}},
      {{F, 68169599, X}, {1972, Feb, 28, 23, 59, 59, X, Mon, 59, UTC, F}},
      {{F, 68169600, 0}, {1972, Feb, 29, 0, 0, 0, 0, Tue, 60, UTC, F}},
      {{F, 68255999, X}, {1972, Feb, 29, 23, 59, 59, X, Tue, 60, UTC, F}},
      {{F, 68256000, 0}, {1972, Mar, 1, 0, 0, 0, 0, Wed, 61, UTC, F}},
      {{F, 94694399, X}, {1972, Dec, 31, 23, 59, 59, X, Sun, 366, UTC, F}},

      // 1973
      {{F, 94694400, 0}, {1973, Jan, 1, 0, 0, 0, 0, Mon, 1, UTC, F}},
      {{F, 99791999, X}, {1973, Feb, 28, 23, 59, 59, X, Wed, 59, UTC, F}},
      {{F, 99792000, 0}, {1973, Mar, 1, 0, 0, 0, 0, Thu, 60, UTC, F}},
      {{F, 126230399, X}, {1973, Dec, 31, 23, 59, 59, X, Mon, 365, UTC, F}},

      // 2000
      {{F, 946684800, 0}, {2000, Jan, 1, 0, 0, 0, 0, Sat, 1, UTC, F}},
      {{F, 951782399, X}, {2000, Feb, 28, 23, 59, 59, X, Mon, 59, UTC, F}},
      {{F, 951782400, 0}, {2000, Feb, 29, 0, 0, 0, 0, Tue, 60, UTC, F}},
      {{F, 951868799, X}, {2000, Feb, 29, 23, 59, 59, X, Tue, 60, UTC, F}},
      {{F, 951868800, 0}, {2000, Mar, 1, 0, 0, 0, 0, Wed, 61, UTC, F}},
      {{F, 978307199, X}, {2000, Dec, 31, 23, 59, 59, X, Sun, 366, UTC, F}},

      // 2001
      {{F, 978307200, 0}, {2001, Jan, 1, 0, 0, 0, 0, Mon, 1, UTC, F}},
      {{F, 978352496, Y}, {2001, Jan, 1, 12, 34, 56, Y, Mon, 1, UTC, F}},
      {{F, 978393600, 0}, {2001, Jan, 2, 0, 0, 0, 0, Tue, 2, UTC, F}},
      {{F, 978825600, 0}, {2001, Jan, 7, 0, 0, 0, 0, Sun, 7, UTC, F}},
      {{F, 978912000, 0}, {2001, Jan, 8, 0, 0, 0, 0, Mon, 8, UTC, F}},
      {{F, 980899200, 0}, {2001, Jan, 31, 0, 0, 0, 0, Wed, 31, UTC, F}},
      {{F, 980985599, X}, {2001, Jan, 31, 23, 59, 59, X, Wed, 31, UTC, F}},
      {{F, 980985600, 0}, {2001, Feb, 1, 0, 0, 0, 0, Thu, 32, UTC, F}},
      {{F, 983404799, X}, {2001, Feb, 28, 23, 59, 59, X, Wed, 59, UTC, F}},
      {{F, 983404800, 0}, {2001, Mar, 1, 0, 0, 0, 0, Thu, 60, UTC, F}},
      {{F, 1009843199, X}, {2001, Dec, 31, 23, 59, 59, X, Mon, 365, UTC, F}},

      // 2002
      {{F, 1009843200, 0}, {2002, Jan, 1, 0, 0, 0, 0, Tue, 1, UTC, F}},

      // 2003
      {{F, 1041379200, 0}, {2003, Jan, 1, 0, 0, 0, 0, Wed, 1, UTC, F}},

      // 2004
      {{F, 1072915200, 0}, {2004, Jan, 1, 0, 0, 0, 0, Thu, 1, UTC, F}},
      {{F, 1078012799, X}, {2004, Feb, 28, 23, 59, 59, X, Sat, 59, UTC, F}},
      {{F, 1078012800, 0}, {2004, Feb, 29, 0, 0, 0, 0, Sun, 60, UTC, F}},
      {{F, 1104537599, X}, {2004, Dec, 31, 23, 59, 59, X, Fri, 366, UTC, F}},

      // 2**31-1
      // {{F, x, X}, {x, Dec, 31, 23, 59, 59, X, Sat, 365, UTC, F}},

      // 1969
      {{T, 0, 1}, {1969, Dec, 31, 23, 59, 59, X, Wed, 365, UTC, F}},
      {{T, 1, 0}, {1969, Dec, 31, 23, 59, 59, 0, Wed, 365, UTC, F}},
      {{T, 86400, 0}, {1969, Dec, 31, 0, 0, 0, 0, Wed, 365, UTC, F}},
      {{T, 86400, 1}, {1969, Dec, 30, 23, 59, 59, X, Tue, 364, UTC, F}},
      {{T, 31536000, 0}, {1969, Jan, 1, 0, 0, 0, 0, Wed, 1, UTC, F}},

      // 0001 (1 CE)
      {{T, 62135596800, 0}, {1, Jan, 1, 0, 0, 0, 0, Mon, 1, UTC, F}},
  };
  for (const auto& row : testdata) {
    auto t = Time::from_epoch(Duration(row.input));
    SCOPED_TRACE(t);
    Breakdown actual;
    actual.set(t);
    EXPECT_PRED_FORMAT2(Matches, row.expected, actual);
  }

#if 0
  auto too_small =
      Time::from_epoch(Duration::from_raw(T, 62135596800ULL, 1U));
  auto too_large = Time::from_epoch(
      Duration::from_raw(F, 67768038274867200ULL, 0U));

  Breakdown b;
  EXPECT_OUT_OF_RANGE(b.set(too_small));
  EXPECT_OK(b.set(too_small + NANOSECOND));
  EXPECT_OK(b.set(too_large - NANOSECOND));
  EXPECT_OUT_OF_RANGE(b.set(too_large));
#endif
}

TEST(Breakdown, SetPacific) {
  struct TestRow {
    internal::DurationRep input;
    Expectation expected;
  };

  auto posixdb = base::time::zone::new_posix_database();
  base::time::zone::Pointer PST8PDT;
  ASSERT_OK(posixdb->get(&PST8PDT, "PST8PDT,M3.2.0,M11.1.0"));

  std::vector<TestRow> testdata = {
      {{F, 0, 0}, {1969, Dec, 31, 16, 0, 0, 0, Wed, 365, PST8PDT, F}},
      {{F, 28800, 0}, {1970, Jan, 1, 0, 0, 0, 0, Thu, 1, PST8PDT, F}},
      {{F, 1199174400, 0}, {2008, Jan, 1, 0, 0, 0, 0, Tue, 1, PST8PDT, F}},
      {{F, 1204271999, X}, {2008, Feb, 28, 23, 59, 59, X, Thu, 59, PST8PDT, F}},
      {{F, 1204272000, 0}, {2008, Feb, 29, 0, 0, 0, 0, Fri, 60, PST8PDT, F}},
      {{F, 1204358399, X}, {2008, Feb, 29, 23, 59, 59, X, Fri, 60, PST8PDT, F}},
      {{F, 1204358400, 0}, {2008, Mar, 1, 0, 0, 0, 0, Sat, 61, PST8PDT, F}},
      {{F, 1205056799, X}, {2008, Mar, 9, 1, 59, 59, X, Sun, 69, PST8PDT, F}},
      {{F, 1205056800, 0}, {2008, Mar, 9, 3, 0, 0, 0, Sun, 69, PST8PDT, T}},
      {{F, 1225612800, 0}, {2008, Nov, 2, 1, 0, 0, 0, Sun, 307, PST8PDT, T}},
      {{F, 1225616399, X}, {2008, Nov, 2, 1, 59, 59, X, Sun, 307, PST8PDT, T}},
      {{F, 1225616400, 0}, {2008, Nov, 2, 1, 0, 0, 0, Sun, 307, PST8PDT, F}},
      {{F, 1225619999, X}, {2008, Nov, 2, 1, 59, 59, X, Sun, 307, PST8PDT, F}},
      {{F, 1225620000, 0}, {2008, Nov, 2, 2, 0, 0, 0, Sun, 307, PST8PDT, F}},
  };
  for (const auto& row : testdata) {
    auto t = Time::from_epoch(Duration(row.input));
    SCOPED_TRACE(t);
    Breakdown actual;
    actual.set(t, PST8PDT);
    EXPECT_PRED_FORMAT2(Matches, row.expected, actual);
  }
}

TEST(Breakdown, SetPacificNoDST) {
  struct TestRow {
    internal::DurationRep input;
    Expectation expected;
  };

  auto posixdb = base::time::zone::new_posix_database();
  base::time::zone::Pointer PST8;
  ASSERT_OK(posixdb->get(&PST8, "PST8"));

  std::vector<TestRow> testdata = {
      {{F, 0, 0}, {1969, Dec, 31, 16, 0, 0, 0, Wed, 365, PST8, F}},
      {{F, 28800, 0}, {1970, Jan, 1, 0, 0, 0, 0, Thu, 1, PST8, F}},
      {{F, 1199174400, 0}, {2008, Jan, 1, 0, 0, 0, 0, Tue, 1, PST8, F}},
      {{F, 1204271999, X}, {2008, Feb, 28, 23, 59, 59, X, Thu, 59, PST8, F}},
      {{F, 1204272000, 0}, {2008, Feb, 29, 0, 0, 0, 0, Fri, 60, PST8, F}},
      {{F, 1204358399, X}, {2008, Feb, 29, 23, 59, 59, X, Fri, 60, PST8, F}},
      {{F, 1204358400, 0}, {2008, Mar, 1, 0, 0, 0, 0, Sat, 61, PST8, F}},
      {{F, 1205056799, X}, {2008, Mar, 9, 1, 59, 59, X, Sun, 69, PST8, F}},
      {{F, 1205056800, 0}, {2008, Mar, 9, 2, 0, 0, 0, Sun, 69, PST8, F}},
      {{F, 1225612800, 0}, {2008, Nov, 2, 0, 0, 0, 0, Sun, 307, PST8, F}},
      {{F, 1225616399, X}, {2008, Nov, 2, 0, 59, 59, X, Sun, 307, PST8, F}},
      {{F, 1225616400, 0}, {2008, Nov, 2, 1, 0, 0, 0, Sun, 307, PST8, F}},
      {{F, 1225619999, X}, {2008, Nov, 2, 1, 59, 59, X, Sun, 307, PST8, F}},
      {{F, 1225620000, 0}, {2008, Nov, 2, 2, 0, 0, 0, Sun, 307, PST8, F}},
  };
  for (const auto& row : testdata) {
    auto t = Time::from_epoch(Duration(row.input));
    SCOPED_TRACE(t);
    Breakdown actual;
    actual.set(t, PST8);
    EXPECT_PRED_FORMAT2(Matches, row.expected, actual);
  }
}

TEST(Breakdown, SetFiji) {
  struct TestRow {
    internal::DurationRep input;
    Expectation expected;
  };

  auto posixdb = base::time::zone::new_posix_database();
  base::time::zone::Pointer FJT;
  ASSERT_OK(posixdb->get(&FJT, "FJT-12FJST,M11.1.0,M1.3.4/75"));

  std::vector<TestRow> testdata = {
      {{F, 0, 0}, {1970, Jan, 1, 13, 0, 0, 0, Thu, 1, FJT, T}},
      {{F, 1199098800, 0}, {2008, Jan, 1, 0, 0, 0, 0, Tue, 1, FJT, T}},
      {{F, 1200747600, 0}, {2008, Jan, 20, 2, 0, 0, 0, Sun, 20, FJT, T}},
      {{F, 1200751199, X}, {2008, Jan, 20, 2, 59, 59, X, Sun, 20, FJT, T}},
      {{F, 1200751200, 0}, {2008, Jan, 20, 2, 0, 0, 0, Sun, 20, FJT, F}},
      {{F, 1200754799, X}, {2008, Jan, 20, 2, 59, 59, X, Sun, 20, FJT, F}},
      {{F, 1200754800, 0}, {2008, Jan, 20, 3, 0, 0, 0, Sun, 20, FJT, F}},
      {{F, 1225547999, X}, {2008, Nov, 2, 1, 59, 59, X, Sun, 307, FJT, F}},
      {{F, 1225548000, 0}, {2008, Nov, 2, 3, 0, 0, 0, Sun, 307, FJT, T}},
  };
  for (const auto& row : testdata) {
    auto t = Time::from_epoch(Duration(row.input));
    SCOPED_TRACE(row.expected);
    Breakdown actual;
    actual.set(t, FJT);
    EXPECT_PRED_FORMAT2(Matches, row.expected, actual);
  }
}
