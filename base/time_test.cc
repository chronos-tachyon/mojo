// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/time.h"

TEST(Time, AsString) {
  base::Time epoch;
  base::Time t = epoch + base::seconds(9000) + base::nanoseconds(1);
  EXPECT_EQ("Time(Duration(false, 9000, 1))", t.as_string());
}
