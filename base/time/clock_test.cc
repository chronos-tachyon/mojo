// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/clock.h"
#include "gtest/gtest.h"

TEST(Clock, Basics) {
  base::time::Clock clock = base::time::system_wallclock();
  EXPECT_TRUE(clock);
  EXPECT_NO_THROW(clock.now());
}

TEST(MonotonicClock, Basics) {
  base::time::MonotonicClock clock = base::time::system_monotonic_clock();
  EXPECT_TRUE(clock);
  EXPECT_NO_THROW(clock.now());
  EXPECT_NO_THROW(clock.convert(base::time::MonotonicTime()));
  EXPECT_NO_THROW(clock.convert(base::time::Time()));
}
