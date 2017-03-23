// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/time.h"
#include "gtest/gtest.h"

static const auto DELTA =
    base::time::seconds(9000) + base::time::nanoseconds(1);

TEST(Time, AsString) {
  base::time::Time epoch;
  auto t = epoch + DELTA;
  auto u = epoch - DELTA;
  EXPECT_EQ("[infinite past]", base::time::Time::min().as_string());
  EXPECT_EQ("[infinite future]", base::time::Time::max().as_string());
  EXPECT_EQ("1970-01-01T00:00:00.000000000Z", epoch.as_string());
  EXPECT_EQ("1970-01-01T02:30:00.000000001Z", t.as_string());
  EXPECT_EQ("1969-12-31T21:29:59.999999999Z", u.as_string());
}

TEST(MonotonicTime, AsString) {
  base::time::MonotonicTime epoch;
  auto t = epoch + DELTA;
  auto u = epoch - DELTA;
  EXPECT_EQ("M+0", epoch.as_string());
  EXPECT_EQ("M+2h30m0.000000001s", t.as_string());
  EXPECT_EQ("M-2h30m0.000000001s", u.as_string());
}
