// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/time.h"

TEST(Time, AsString) {
  base::Duration delta = base::seconds(9000) + base::nanoseconds(1);
  base::Time epoch;
  base::Time t = epoch + delta;
  base::Time u = epoch - delta;
  EXPECT_EQ("T+0", epoch.as_string());
  EXPECT_EQ("T+2h30m0.000000001s", t.as_string());
  EXPECT_EQ("T-2h30m0.000000001s", u.as_string());
}

TEST(MonotonicTime, AsString) {
  base::Duration delta = base::seconds(9000) + base::nanoseconds(1);
  base::MonotonicTime epoch;
  base::MonotonicTime t = epoch + delta;
  base::MonotonicTime u = epoch - delta;
  EXPECT_EQ("M+0", epoch.as_string());
  EXPECT_EQ("M+2h30m0.000000001s", t.as_string());
  EXPECT_EQ("M-2h30m0.000000001s", u.as_string());
}
