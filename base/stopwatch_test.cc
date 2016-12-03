// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/stopwatch.h"

struct FakeClock : public base::MonotonicClockImpl {
  base::MonotonicTime t;

  base::MonotonicTime now() const override { return t; }
  base::MonotonicTime convert(base::Time t) const override {
    throw std::logic_error("not implemented");
  }
  base::Time convert(base::MonotonicTime t) const override {
    throw std::logic_error("not implemented");
  }
};

TEST(Stopwatch, EndToEnd) {
  auto fc = std::make_shared<FakeClock>();
  base::MonotonicClock c(fc);

  EXPECT_EQ(fc->t, c.now());
  fc->t += base::seconds(1);
  EXPECT_EQ(fc->t, c.now());

  base::Stopwatch w(c);
  w.start();
  fc->t += base::seconds(3);
  EXPECT_EQ(base::seconds(3), w.elapsed());
  EXPECT_EQ(base::seconds(3), w.cumulative());
  fc->t += base::seconds(2);
  EXPECT_EQ(base::seconds(5), w.elapsed());
  EXPECT_EQ(base::seconds(5), w.cumulative());
  w.stop();
  fc->t += base::seconds(1);
  EXPECT_EQ(base::seconds(5), w.elapsed());
  EXPECT_EQ(base::seconds(5), w.cumulative());
  w.start();
  fc->t += base::seconds(17);
  EXPECT_EQ(base::seconds(17), w.elapsed());
  EXPECT_EQ(base::seconds(22), w.cumulative());
  w.stop();
  fc->t += base::seconds(7);
  EXPECT_EQ(base::seconds(17), w.elapsed());
  EXPECT_EQ(base::seconds(22), w.cumulative());
  w.reset();
  EXPECT_EQ(base::Duration(), w.elapsed());
  EXPECT_EQ(base::Duration(), w.cumulative());
}
