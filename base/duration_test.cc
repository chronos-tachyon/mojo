// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/duration.h"

using base::Duration;

static Duration make(bool neg, base::safe<uint64_t> s,
                     base::safe<uint32_t> ns) {
  return Duration::from_raw(neg, s, ns);
}

TEST(Duration, Basics) {
  auto d1 = base::minutes(5);
  EXPECT_EQ(make(false, 300U, 0U), d1);
  EXPECT_EQ(5, d1.minutes());
  EXPECT_EQ(300, d1.seconds());
  EXPECT_EQ(300000, d1.milliseconds());
  EXPECT_EQ(300000000, d1.microseconds());
  EXPECT_EQ(300000000000, d1.nanoseconds());

  auto d2 = base::seconds(1);
  EXPECT_EQ(make(false, 1U, 0U), d2);
  EXPECT_EQ(0, d2.minutes());
  EXPECT_EQ(1, d2.seconds());
  EXPECT_EQ(1000, d2.milliseconds());
  EXPECT_EQ(1000000, d2.microseconds());
  EXPECT_EQ(1000000000, d2.nanoseconds());
  d2 *= 300U;
  EXPECT_EQ(d1, d2);

  d2 = base::seconds(1);
  auto d3 = base::milliseconds(250);
  auto d4 = d3;
  EXPECT_EQ(make(false, 0U, 250000000U), d3);
  d3 *= 4;
  EXPECT_EQ(d2, d3);

  d4 *= 7;
  EXPECT_EQ(make(false, 1U, 750000000U), d4);
}

TEST(Duration, Negation) {
  auto a = base::Duration::from_raw(false, 0U, 0U);
  auto b = -a;
  EXPECT_EQ(make(false, 0U, 0U), b);

  a = base::Duration::from_raw(false, 0U, 1U);
  b = -a;
  EXPECT_EQ(make(true, 0U, 1U), b);

  a = base::Duration::from_raw(false, 1U, 0U);
  b = -a;
  EXPECT_EQ(make(true, 1U, 0U), b);

  a = base::Duration::from_raw(true, 0U, 1U);
  b = -a;
  EXPECT_EQ(make(false, 0U, 1U), b);

  a = base::Duration::from_raw(true, 1U, 0U);
  b = -a;
  EXPECT_EQ(make(false, 1U, 0U), b);
}

TEST(Duration, AdditionSubtraction) {
  auto a = base::minutes(5);
  auto b = base::minutes(3);
  auto c = a + b;
  EXPECT_EQ(base::minutes(8), c);
  c = a - b;
  EXPECT_EQ(base::minutes(2), c);
}

TEST(Duration, ScalarMultiplicationDivision) {
  auto a = base::seconds(1);
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
}

TEST(Duration, RatioDivision) {
  auto a = base::seconds(5);
  auto b = base::seconds(2);
  EXPECT_EQ(2.5, a / b);
  EXPECT_EQ(0.4, b / a);
  auto c = base::seconds(1);
  EXPECT_EQ(2.0, divmod(a, b).first);
  EXPECT_EQ(0.0, divmod(b, a).first);
  EXPECT_EQ(c, a % b);
  EXPECT_EQ(b, b % a);
}

TEST(Duration, AsString) {
  EXPECT_EQ("Duration(false, 2, 750000000)",
            base::milliseconds(2750).as_string());
  EXPECT_EQ("Duration(true, 1, 250000000)",
            base::milliseconds(-1250).as_string());
}
