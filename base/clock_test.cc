// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/clock.h"

TEST(Clock, Basics) {
  base::Clock clock = base::system_wallclock();
  EXPECT_TRUE(clock);
  EXPECT_NO_THROW(clock.now());
}

TEST(MonotonicClock, Basics) {
  base::MonotonicClock clock = base::system_monotonic_clock();
  EXPECT_TRUE(clock);
  EXPECT_NO_THROW(clock.now());
  EXPECT_NO_THROW(clock.convert(base::MonotonicTime()));
  EXPECT_NO_THROW(clock.convert(base::Time()));
}
