// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/duration.h"
#include "gtest/gtest.h"

using base::time::Duration;
using base::time::hours;
using base::time::minutes;
using base::time::seconds;
using base::time::milliseconds;

static Duration make(bool neg, base::safe<uint64_t> s,
                     base::safe<uint32_t> ns) {
  return Duration::from_raw(neg, s, ns);
}

TEST(Duration, Basics) {
  auto d1 = minutes(5);
  EXPECT_EQ(make(false, 300U, 0U), d1);
  EXPECT_EQ(5, d1.minutes());
  EXPECT_EQ(300, d1.seconds());
  EXPECT_EQ(300000, d1.milliseconds());
  EXPECT_EQ(300000000, d1.microseconds());
  EXPECT_EQ(300000000000, d1.nanoseconds());

  auto d2 = seconds(1);
  EXPECT_EQ(make(false, 1U, 0U), d2);
  EXPECT_EQ(0, d2.minutes());
  EXPECT_EQ(1, d2.seconds());
  EXPECT_EQ(1000, d2.milliseconds());
  EXPECT_EQ(1000000, d2.microseconds());
  EXPECT_EQ(1000000000, d2.nanoseconds());
  d2 *= 300U;
  EXPECT_EQ(d1, d2);

  d2 = seconds(1);
  auto d3 = milliseconds(250);
  auto d4 = d3;
  EXPECT_EQ(make(false, 0U, 250000000U), d3);
  d3 *= 4;
  EXPECT_EQ(d2, d3);

  d4 *= 7;
  EXPECT_EQ(make(false, 1U, 750000000U), d4);
}

TEST(Duration, Negation) {
  auto a = Duration::from_raw(false, 0U, 0U);
  auto b = -a;
  EXPECT_EQ(make(false, 0U, 0U), b);

  a = Duration::from_raw(false, 0U, 1U);
  b = -a;
  EXPECT_EQ(make(true, 0U, 1U), b);

  a = Duration::from_raw(false, 1U, 0U);
  b = -a;
  EXPECT_EQ(make(true, 1U, 0U), b);

  a = Duration::from_raw(true, 0U, 1U);
  b = -a;
  EXPECT_EQ(make(false, 0U, 1U), b);

  a = Duration::from_raw(true, 1U, 0U);
  b = -a;
  EXPECT_EQ(make(false, 1U, 0U), b);
}

TEST(Duration, AdditionSubtraction) {
  auto a = minutes(5);
  auto b = minutes(3);
  auto c = a + b;
  EXPECT_EQ(minutes(8), c);
  c = a - b;
  EXPECT_EQ(minutes(2), c);
}

TEST(Duration, ScalarMultiplicationDivision) {
  auto a = seconds(1);
  EXPECT_EQ(make(false, 0U, 0U), 0 * a);
  EXPECT_EQ(make(false, 1U, 0U), 1 * a);
  EXPECT_EQ(make(true, 1U, 0U), -1 * a);

  EXPECT_EQ(make(false, 0U, 0U), 0.0 * a);
  EXPECT_EQ(make(false, 1U, 0U), 1.0 * a);
  EXPECT_EQ(make(true, 1U, 0U), -1.0 * a);
  EXPECT_EQ(make(false, 2U, 500000000U), 2.5 * a);

  auto b = 5 * a;
  EXPECT_EQ(make(false, 1U, 0U), b / 5);
  EXPECT_EQ(make(false, 2U, 500000000U), b / 2);
  EXPECT_EQ(make(false, 2U, 0U), b / 2.5);

  auto c = minutes(1) + seconds(20);
  auto d = c * 3;
  EXPECT_EQ(make(false, 80U, 0U), c);
  EXPECT_EQ(make(false, 240U, 0U), d);
  EXPECT_EQ(minutes(4), d);
  EXPECT_EQ(minutes(4) / 3, c);
  EXPECT_EQ(minutes(4) / 3.0, c);
  EXPECT_EQ(minutes(4.0 / 3.0), c);
}

TEST(Duration, RatioDivision) {
  auto a = seconds(5);
  auto b = seconds(2);
  EXPECT_EQ(2.5, a / b);
  EXPECT_EQ(0.4, b / a);
  auto c = seconds(1);
  EXPECT_EQ(2.0, divmod(a, b).first);
  EXPECT_EQ(0.0, divmod(b, a).first);
  EXPECT_EQ(c, a % b);
  EXPECT_EQ(b, b % a);
}

TEST(Duration, AsString) {
  EXPECT_EQ("1h30m", hours(1.5).as_string());
  EXPECT_EQ("15m", hours(0.25).as_string());
  EXPECT_EQ("1m20s", minutes(4.0 / 3.0).as_string());
  EXPECT_EQ("2.75s", milliseconds(2750).as_string());
  EXPECT_EQ("-1.25s", milliseconds(-1250).as_string());
  EXPECT_EQ("750ms", milliseconds(750).as_string());
  EXPECT_EQ("500µs", milliseconds(0.5).as_string());
  EXPECT_EQ("500ns", milliseconds(0.0005).as_string());
}
