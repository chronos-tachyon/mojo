// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/time/stopwatch.h"
#include "gtest/gtest.h"

using base::time::Duration;
using base::time::MonotonicClock;
using base::time::MonotonicClockImpl;
using base::time::MonotonicTime;
using base::time::Stopwatch;
using base::time::Time;
using base::time::seconds;

struct FakeClock : public MonotonicClockImpl {
  MonotonicTime t;

  MonotonicTime now() const override { return t; }
  MonotonicTime convert(Time t) const override {
    throw std::logic_error("not implemented");
  }
  Time convert(MonotonicTime t) const override {
    throw std::logic_error("not implemented");
  }
};

TEST(Stopwatch, EndToEnd) {
  auto fc = std::make_shared<FakeClock>();
  MonotonicClock c(fc);

  EXPECT_EQ(fc->t, c.now());
  fc->t += seconds(1);
  EXPECT_EQ(fc->t, c.now());

  Stopwatch w(c);
  w.start();
  fc->t += seconds(3);
  EXPECT_EQ(seconds(3), w.elapsed());
  EXPECT_EQ(seconds(3), w.cumulative());
  fc->t += seconds(2);
  EXPECT_EQ(seconds(5), w.elapsed());
  EXPECT_EQ(seconds(5), w.cumulative());
  w.stop();
  fc->t += seconds(1);
  EXPECT_EQ(seconds(5), w.elapsed());
  EXPECT_EQ(seconds(5), w.cumulative());
  w.start();
  fc->t += seconds(17);
  EXPECT_EQ(seconds(17), w.elapsed());
  EXPECT_EQ(seconds(22), w.cumulative());
  w.stop();
  fc->t += seconds(7);
  EXPECT_EQ(seconds(17), w.elapsed());
  EXPECT_EQ(seconds(22), w.cumulative());
  w.reset();
  EXPECT_EQ(Duration(), w.elapsed());
  EXPECT_EQ(Duration(), w.cumulative());
}
