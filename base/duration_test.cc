// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/duration.h"

TEST(Duration, Basics) {
  auto d1 = base::minutes(5);
  EXPECT_EQ(std::make_tuple(false, 300U, 0U), d1.raw());
  EXPECT_EQ(5, d1.minutes());
  EXPECT_EQ(300, d1.seconds());
  EXPECT_EQ(300000, d1.milliseconds());
  EXPECT_EQ(300000000, d1.microseconds());
  EXPECT_EQ(300000000000, d1.nanoseconds());

  auto d2 = base::seconds(1);
  EXPECT_EQ(std::make_tuple(false, 1U, 0U), d2.raw());
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
  EXPECT_EQ(std::make_tuple(false, 0U, 250000000U), d3.raw());
  d3 *= 4;
  EXPECT_EQ(d2, d3);

  d4 *= 7;
  EXPECT_EQ(std::make_tuple(false, 1U, 750000000U), d4.raw());
}

TEST(Duration, AsString) {
  EXPECT_EQ("Duration(false, 2, 750000000)",
            base::milliseconds(2750).as_string());
  EXPECT_EQ("Duration(true, 1, 250000000)",
            base::milliseconds(-1250).as_string());
}
