// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include <ostream>

#include "base/concat.h"
#include "base/result_testing.h"
#include "base/time/breakdown.h"
#include "base/time/duration.h"
#include "base/time/time.h"
#include "base/time/zone.h"
#include "gtest/gtest.h"

using base::time::Time;
using base::time::Duration;
using base::time::internal::DurationRep;

using base::time::zone::Type;
using base::time::zone::Regime;
using base::time::zone::Recurrence;
using base::time::zone::LeapSecond;

using M = base::time::zone::Recurrence::Mode;

static std::ostream& operator<<(std::ostream& o, const Recurrence& rec) {
  switch (rec.mode()) {
    case M::never:
      o << "never";
      break;

    case M::always:
      o << "always";
      break;

    case M::julian0:
      o << rec.day();
      break;

    case M::julian1:
      o << "J" << rec.day();
      break;

    case M::month_week_wday:
      o << "M" << rec.month() << "." << rec.week() << "." << rec.day();
      break;
  }
  o << "/" << rec.seconds_past_midnight();
  return o;
}

TEST(Posix, GetZone) {
  auto posix = base::time::zone::new_posix_database();
  base::time::zone::Pointer zone;
  const Type* type0;
  const Type* type1;
  const Regime* regime;

  const auto MIN = Time::min();
  const auto MAX = Time::max();

  const auto NEVER = Recurrence(M::never, 0, 0, 0, 0);
  const auto ALWAYS = Recurrence(M::always, 0, 0, 0, 0);
  const auto M3_2_0 = Recurrence(M::month_week_wday, 3, 2, 0, 7200);
  const auto M11_1_0 = Recurrence(M::month_week_wday, 11, 1, 0, 7200);
  const auto M1_3_4_75 = Recurrence(M::month_week_wday, 1, 3, 4, 270000);

  ASSERT_OK(posix->get(&zone, "UTC0"));
  ASSERT_GE(zone->types().size(), 1U);
  EXPECT_EQ(zone->types().size(), 1U);
  type0 = &zone->types()[0];
  EXPECT_EQ("UTC", type0->abbreviation());
  EXPECT_EQ(0, type0->utc_offset());
  EXPECT_FALSE(type0->is_dst());
  EXPECT_TRUE(type0->is_specified());
  ASSERT_EQ(zone->regimes().size(), 1U);
  regime = &zone->regimes()[0];
  EXPECT_EQ(MIN, regime->regime_begin());
  EXPECT_EQ(MAX, regime->regime_end());
  EXPECT_EQ(NEVER, regime->dst_begin());
  EXPECT_EQ(ALWAYS, regime->dst_end());
  EXPECT_TRUE(regime->standard_time() == type0);
  EXPECT_TRUE(regime->daylight_time() == type0);

  ASSERT_OK(posix->get(&zone, "PST8PDT"));
  ASSERT_EQ(zone->types().size(), 2U);
  type0 = &zone->types()[0];
  EXPECT_EQ("PST", type0->abbreviation());
  EXPECT_EQ(-28800, type0->utc_offset());
  EXPECT_FALSE(type0->is_dst());
  EXPECT_TRUE(type0->is_specified());
  type1 = &zone->types()[1];
  EXPECT_EQ("PDT", type1->abbreviation());
  EXPECT_EQ(-25200, type1->utc_offset());
  EXPECT_TRUE(type1->is_dst());
  EXPECT_TRUE(type1->is_specified());
  ASSERT_EQ(zone->regimes().size(), 1U);
  regime = &zone->regimes()[0];
  EXPECT_EQ(MIN, regime->regime_begin());
  EXPECT_EQ(MAX, regime->regime_end());
  EXPECT_EQ(M3_2_0, regime->dst_begin());
  EXPECT_EQ(M11_1_0, regime->dst_end());
  EXPECT_TRUE(regime->standard_time() == type0);
  EXPECT_TRUE(regime->daylight_time() == type1);

  ASSERT_OK(posix->get(&zone, "FJT-12FJST,M11.1.0,M1.3.4/75"));
  ASSERT_EQ(zone->types().size(), 2U);
  type0 = &zone->types()[0];
  EXPECT_EQ("FJT", type0->abbreviation());
  EXPECT_EQ(43200, type0->utc_offset());
  EXPECT_FALSE(type0->is_dst());
  EXPECT_TRUE(type0->is_specified());
  type1 = &zone->types()[1];
  EXPECT_EQ("FJST", type1->abbreviation());
  EXPECT_EQ(46800, type1->utc_offset());
  EXPECT_TRUE(type1->is_dst());
  EXPECT_TRUE(type1->is_specified());
  ASSERT_EQ(zone->regimes().size(), 1U);
  regime = &zone->regimes()[0];
  EXPECT_EQ(MIN, regime->regime_begin());
  EXPECT_EQ(MAX, regime->regime_end());
  EXPECT_EQ(M11_1_0, regime->dst_begin());
  EXPECT_EQ(M1_3_4_75, regime->dst_end());
  EXPECT_TRUE(regime->standard_time() == type0);
  EXPECT_TRUE(regime->daylight_time() == type1);

  ASSERT_OK(posix->get(&zone, "WART4WARST,J1/0,J365/25"));
  ASSERT_EQ(zone->types().size(), 2U);
  type0 = &zone->types()[0];
  EXPECT_EQ("WART", type0->abbreviation());
  EXPECT_EQ(-14400, type0->utc_offset());
  EXPECT_FALSE(type0->is_dst());
  EXPECT_TRUE(type0->is_specified());
  type1 = &zone->types()[1];
  EXPECT_EQ("WARST", type1->abbreviation());
  EXPECT_EQ(-10800, type1->utc_offset());
  EXPECT_TRUE(type1->is_dst());
  EXPECT_TRUE(type1->is_specified());
  ASSERT_EQ(zone->regimes().size(), 1U);
  regime = &zone->regimes()[0];
  EXPECT_EQ(MIN, regime->regime_begin());
  EXPECT_EQ(MAX, regime->regime_end());
  EXPECT_EQ(ALWAYS, regime->dst_begin());
  EXPECT_EQ(NEVER, regime->dst_end());
  EXPECT_TRUE(regime->standard_time() == type1);
  EXPECT_TRUE(regime->daylight_time() == type1);
}

TEST(ZoneInfo, GetZone) {
  const char* srcdir = ::getenv("TEST_SRCDIR");
  ASSERT_TRUE(srcdir != nullptr);

  std::string tzdir = base::concat(
      srcdir, "/com_github_chronostachyon_mojo/base/time/testdata");
  auto posix = base::time::zone::new_zoneinfo_database(tzdir);
  base::time::zone::Pointer zone;
  const Type *type0, *type1, *type2, *type3, *type4;
  const Regime* regime;

  const auto MIN = Time::min();
  const auto MAX = Time::max();
  const auto T0 = Time(Duration(DurationRep(true, 0xa1fbe540ULL, 0)));
  const auto T1 = Time(Duration(DurationRep(false, 0x7db84890ULL, 0)));
  const auto T2 = Time(Duration(DurationRep(false, 0x7e5e73a0ULL, 0)));
  const auto T3 = Time(Duration(DurationRep(false, 0x7f982a90ULL, 0)));

  const auto NEVER = Recurrence(M::never, 0, 0, 0, 0);
  const auto ALWAYS = Recurrence(M::always, 0, 0, 0, 0);
  const auto M3_2_0 = Recurrence(M::month_week_wday, 3, 2, 0, 7200);
  const auto M11_1_0 = Recurrence(M::month_week_wday, 11, 1, 0, 7200);

  ASSERT_OK(posix->get(&zone, "UTC"));
  ASSERT_EQ(zone->types().size(), 1U);

  type0 = &zone->types()[0];
  EXPECT_EQ("UTC", type0->abbreviation());
  EXPECT_EQ(0, type0->utc_offset());
  EXPECT_FALSE(type0->is_dst());
  EXPECT_TRUE(type0->is_specified());

  ASSERT_EQ(zone->regimes().size(), 1U);

  regime = &zone->regimes()[0];
  EXPECT_EQ(MIN, regime->regime_begin());
  EXPECT_EQ(MAX, regime->regime_end());
  EXPECT_EQ(NEVER, regime->dst_begin());
  EXPECT_EQ(ALWAYS, regime->dst_end());
  EXPECT_TRUE(regime->standard_time() == type0);
  EXPECT_TRUE(regime->daylight_time() == type0);

  ASSERT_OK(posix->get(&zone, "America.Los_Angeles"));
  ASSERT_EQ(zone->types().size(), 5U);

  type0 = &zone->types()[0];
  EXPECT_EQ("LMT", type0->abbreviation());
  EXPECT_EQ(-28378, type0->utc_offset());
  EXPECT_FALSE(type0->is_dst());
  EXPECT_TRUE(type0->is_specified());

  type1 = &zone->types()[1];
  EXPECT_EQ("PDT", type1->abbreviation());
  EXPECT_EQ(-25200, type1->utc_offset());
  EXPECT_TRUE(type1->is_dst());
  EXPECT_TRUE(type1->is_specified());

  type2 = &zone->types()[2];
  EXPECT_EQ("PST", type2->abbreviation());
  EXPECT_EQ(-28800, type2->utc_offset());
  EXPECT_FALSE(type2->is_dst());
  EXPECT_TRUE(type2->is_specified());

  type3 = &zone->types()[3];
  EXPECT_EQ("PWT", type3->abbreviation());
  EXPECT_EQ(-25200, type3->utc_offset());
  EXPECT_TRUE(type3->is_dst());
  EXPECT_TRUE(type3->is_specified());

  type4 = &zone->types()[4];
  EXPECT_EQ("PPT", type4->abbreviation());
  EXPECT_EQ(-25200, type4->utc_offset());
  EXPECT_TRUE(type4->is_dst());
  EXPECT_TRUE(type4->is_specified());

  ASSERT_GE(zone->regimes().size(), 3U);

  regime = &zone->regimes().front();
  EXPECT_EQ(MIN, regime->regime_begin());
  EXPECT_EQ(T0, regime->regime_end());
  EXPECT_EQ(NEVER, regime->dst_begin());
  EXPECT_EQ(ALWAYS, regime->dst_end());
  EXPECT_TRUE(regime->standard_time() == type0);
  EXPECT_TRUE(regime->daylight_time() == type0);

  regime = &zone->regimes()[zone->regimes().size() - 3];
  EXPECT_EQ(T1, regime->regime_begin());
  EXPECT_EQ(T2, regime->regime_end());
  EXPECT_EQ(NEVER, regime->dst_begin());
  EXPECT_EQ(ALWAYS, regime->dst_end());
  EXPECT_TRUE(regime->standard_time() == type2);
  EXPECT_TRUE(regime->daylight_time() == type1);

  regime = &zone->regimes()[zone->regimes().size() - 2];
  EXPECT_EQ(T2, regime->regime_begin());
  EXPECT_EQ(T3, regime->regime_end());
  EXPECT_EQ(ALWAYS, regime->dst_begin());
  EXPECT_EQ(NEVER, regime->dst_end());
  EXPECT_TRUE(regime->standard_time() == type2);
  EXPECT_TRUE(regime->daylight_time() == type1);

  regime = &zone->regimes().back();
  EXPECT_EQ(T3, regime->regime_begin());
  EXPECT_EQ(MAX, regime->regime_end());
  EXPECT_EQ(M3_2_0, regime->dst_begin());
  EXPECT_EQ(M11_1_0, regime->dst_end());
  EXPECT_TRUE(regime->standard_time() == type2);
  EXPECT_TRUE(regime->daylight_time() == type1);
}
