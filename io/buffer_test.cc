// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <string>

#include "io/buffer.h"

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

TEST(Pool, EndToEnd) {
  io::PoolPtr pool = io::make_pool(4096, 2);
  EXPECT_EQ(4096U, pool->buffer_size());
  EXPECT_EQ(2U, pool->max());
  EXPECT_EQ(0U, pool->size());

  pool->reserve(2);
  EXPECT_EQ(2U, pool->size());

  std::string expected(4096, '\0');

  io::OwnedBuffer x = pool->take();
  EXPECT_EQ(1U, pool->size());
  EXPECT_EQ(4096U, x.size());
  EXPECT_EQ(expected, std::string(x.data(), x.size()));

  io::OwnedBuffer y = pool->take();
  EXPECT_EQ(0U, pool->size());
  EXPECT_EQ(4096U, y.size());
  EXPECT_EQ(expected, std::string(y.data(), y.size()));
  EXPECT_TRUE(x.data() != y.data());

  io::OwnedBuffer z = pool->take();
  EXPECT_EQ(0U, pool->size());
  EXPECT_EQ(4096U, z.size());
  EXPECT_EQ(expected, std::string(z.data(), z.size()));
  EXPECT_TRUE(x.data() != z.data());
  EXPECT_TRUE(y.data() != z.data());

  pool->give(std::move(x));
  EXPECT_EQ(1U, pool->size());

  pool->give(std::move(y));
  EXPECT_EQ(2U, pool->size());

  pool->give(std::move(z));
  EXPECT_EQ(2U, pool->size());
}
