// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/cleanup.h"

struct IncCleanupFunction {
  int* pointer;

  explicit IncCleanupFunction(int* p) noexcept : pointer(p) {}
  void operator()() noexcept { ++*pointer; }
};

TEST(Cleanup, Flow) {
  // Runs the code at all
  int a = 0;
  base::cleanup([&] { ++a; });  // closure
  EXPECT_EQ(1, a);
  base::cleanup(IncCleanupFunction(&a));  // functor
  EXPECT_EQ(2, a);

  // Runs at destruction time
  a = 0;
  {
    auto cleanup = base::cleanup(IncCleanupFunction(&a));
    EXPECT_EQ(0, a);
  }
  EXPECT_EQ(1, a);

  // Can be cancelled
  a = 0;
  {
    auto cleanup = base::cleanup(IncCleanupFunction(&a));
    EXPECT_EQ(0, a);
    EXPECT_TRUE(cleanup);
    cleanup.cancel();
    EXPECT_EQ(0, a);
    EXPECT_FALSE(cleanup);
  }
  EXPECT_EQ(0, a);

  // Can be run prematurely (and runs at most once)
  a = 0;
  {
    auto cleanup = base::cleanup(IncCleanupFunction(&a));
    EXPECT_EQ(0, a);
    EXPECT_TRUE(cleanup);
    cleanup.run();
    EXPECT_EQ(1, a);
    EXPECT_FALSE(cleanup);
  }
  EXPECT_EQ(1, a);

  // Cleanup function can be changed iff types are compatible
  a = 42;
  int b = 23;
  {
    auto cleanup = base::cleanup(IncCleanupFunction(&a));
    EXPECT_EQ(42, a);
    EXPECT_EQ(23, b);
    EXPECT_TRUE(cleanup);
    cleanup = IncCleanupFunction(&b);
    EXPECT_EQ(42, a);
    EXPECT_EQ(23, b);
    EXPECT_TRUE(cleanup);
  }
  EXPECT_EQ(42, a);
  EXPECT_EQ(24, b);
}
