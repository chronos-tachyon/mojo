// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/time.h"
#include "gtest/gtest.h"

using base::time::Duration;
using base::time::Time;
using base::time::MonotonicTime;

TEST(Time, AsString) {
  Duration delta = base::time::seconds(9000) + base::time::nanoseconds(1);
  Time epoch;
  Time t = epoch + delta;
  Time u = epoch - delta;
  EXPECT_EQ("T+0", epoch.as_string());
  EXPECT_EQ("T+2h30m0.000000001s", t.as_string());
  EXPECT_EQ("T-2h30m0.000000001s", u.as_string());
}

TEST(MonotonicTime, AsString) {
  Duration delta = base::time::seconds(9000) + base::time::nanoseconds(1);
  MonotonicTime epoch;
  MonotonicTime t = epoch + delta;
  MonotonicTime u = epoch - delta;
  EXPECT_EQ("M+0", epoch.as_string());
  EXPECT_EQ("M+2h30m0.000000001s", t.as_string());
  EXPECT_EQ("M-2h30m0.000000001s", u.as_string());
}
