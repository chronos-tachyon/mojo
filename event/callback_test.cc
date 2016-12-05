// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/result_testing.h"
#include "event/callback.h"

TEST(Callback, Basics) {
  std::unique_ptr<event::Callback> c;

  int a = 0;
  std::function<base::Result()> f = [&a] {
    ++a;
    return base::Result();
  };

  c = event::callback(std::move(f));
  EXPECT_EQ(0, a);
  EXPECT_OK(c->run());
  EXPECT_EQ(1, a);

  auto closure0 = [&a](int* ptr) {
    ++a;
    *ptr /= 2;
    return base::Result::out_of_range("my spoon is too big");
  };

  int b = 8;
  c = event::callback(closure0, &b);
  EXPECT_EQ(1, a);
  EXPECT_EQ(8, b);
  EXPECT_OUT_OF_RANGE(c->run());
  EXPECT_EQ(2, a);
  EXPECT_EQ(4, b);

  auto closure1 = [&a](std::unique_ptr<int> ptr) {
    if (!ptr) return base::Result::internal("null pointer");
    a += *ptr;
    return base::Result();
  };

  std::unique_ptr<int> dummy(new int(42));
  c = event::callback(closure1, std::move(dummy));
  EXPECT_EQ(2, a);
  EXPECT_OK(c->run());
  EXPECT_EQ(44, a);
  EXPECT_INTERNAL(c->run());
  EXPECT_EQ(44, a);
}
