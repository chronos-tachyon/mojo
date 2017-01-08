// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "file/registry.h"

TEST(Linker, LocalFS) {
  auto fs0 = file::system_registry().find("local");
  EXPECT_TRUE(!!fs0);
}

TEST(Linker, MemFS) {
  auto fs0 = file::system_registry().find("mem");
  EXPECT_TRUE(!!fs0);
}
