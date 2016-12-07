// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <string>

#include "io/buffer.h"

TEST(NextPow8, Basics) {
  EXPECT_EQ(1U, io::next_power_of_two(0U));
  EXPECT_EQ(1U, io::next_power_of_two(1U));
  EXPECT_EQ(2U, io::next_power_of_two(2U));
  EXPECT_EQ(4U, io::next_power_of_two(3U));
  EXPECT_EQ(4U, io::next_power_of_two(4U));
  EXPECT_EQ(8U, io::next_power_of_two(5U));
  EXPECT_EQ(8U, io::next_power_of_two(7U));
  EXPECT_EQ(8U, io::next_power_of_two(8U));
  EXPECT_EQ(16U, io::next_power_of_two(9U));
  EXPECT_EQ(16U, io::next_power_of_two(15U));
  EXPECT_EQ(16U, io::next_power_of_two(16U));
  EXPECT_EQ(32U, io::next_power_of_two(17U));
  EXPECT_EQ(32U, io::next_power_of_two(31U));
  EXPECT_EQ(32U, io::next_power_of_two(32U));
  EXPECT_EQ(64U, io::next_power_of_two(33U));
  EXPECT_EQ(64U, io::next_power_of_two(63U));
  EXPECT_EQ(64U, io::next_power_of_two(64U));
}

TEST(OwnedBuffer, Move) {
  io::OwnedBuffer buf0 = io::OwnedBuffer(64);
  EXPECT_TRUE(buf0.data() != nullptr);
  EXPECT_EQ(64U, buf0.size());

  io::OwnedBuffer buf1(std::move(buf0));
  EXPECT_TRUE(buf0.data() == nullptr);
  EXPECT_EQ(0U, buf0.size());
  EXPECT_TRUE(buf1.data() != nullptr);
  EXPECT_EQ(64U, buf1.size());

  io::OwnedBuffer buf2;
  EXPECT_TRUE(buf2.data() == nullptr);
  EXPECT_EQ(0U, buf2.size());

  buf2 = std::move(buf1);
  EXPECT_TRUE(buf1.data() == nullptr);
  EXPECT_EQ(0U, buf1.size());
  EXPECT_TRUE(buf2.data() != nullptr);
  EXPECT_EQ(64U, buf2.size());
}

TEST(BufferPool, Null) {
  io::BufferPool pool(4096, io::null_pool);
  EXPECT_EQ(0U, pool.pool_size());
  EXPECT_EQ(0U, pool.pool_max());

  std::string expected(4096, '\0');

  io::OwnedBuffer x = pool.take();
  EXPECT_EQ(0U, pool.pool_size());
  EXPECT_EQ(0U, pool.pool_max());
  EXPECT_EQ(4096U, x.size());
  EXPECT_EQ(expected, std::string(x.data(), x.size()));

  io::OwnedBuffer y = pool.take();
  EXPECT_EQ(0U, pool.pool_size());
  EXPECT_EQ(0U, pool.pool_max());
  EXPECT_EQ(4096U, y.size());
  EXPECT_EQ(expected, std::string(y.data(), y.size()));
  EXPECT_TRUE(x.data() != y.data());

  pool.give(std::move(x));
  EXPECT_EQ(0U, pool.pool_size());
  EXPECT_EQ(0U, pool.pool_max());

  pool.give(std::move(y));
  EXPECT_EQ(0U, pool.pool_size());
  EXPECT_EQ(0U, pool.pool_max());
}

TEST(BufferPool, NotNull) {
  io::BufferPool pool(4096);
  EXPECT_EQ(0U, pool.pool_size());
  EXPECT_EQ(16U, pool.pool_max());

  pool.reserve(2);
  EXPECT_EQ(2U, pool.pool_size());
  EXPECT_EQ(16U, pool.pool_max());

  std::string expected(4096, '\0');

  io::OwnedBuffer x = pool.take();
  EXPECT_EQ(1U, pool.pool_size());
  EXPECT_EQ(16U, pool.pool_max());
  EXPECT_EQ(4096U, x.size());
  EXPECT_EQ(expected, std::string(x.data(), x.size()));

  io::OwnedBuffer y = pool.take();
  EXPECT_EQ(0U, pool.pool_size());
  EXPECT_EQ(16U, pool.pool_max());
  EXPECT_EQ(4096U, y.size());
  EXPECT_EQ(expected, std::string(y.data(), y.size()));
  EXPECT_TRUE(x.data() != y.data());

  pool.give(std::move(x));
  EXPECT_EQ(1U, pool.pool_size());
  EXPECT_EQ(16U, pool.pool_max());

  pool.give(std::move(y));
  EXPECT_EQ(2U, pool.pool_size());
  EXPECT_EQ(16U, pool.pool_max());
}
