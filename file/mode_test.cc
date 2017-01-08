// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "file/mode.h"

TEST(Mode, AsString) {
  struct TestItem {
    std::string str;
    uint16_t bits;
    bool valid;
  };

  std::vector<TestItem> testdata{
      {"r", 0x01, true},
      {"rw", 0x03, true},
      {"wt", 0x22, true},
      {"wcx", 0x1a, true},
      {"wct", 0x2a, true},
      {"wa", 0x06, true},
  };

  for (const auto& row : testdata) {
    file::Mode mode(row.str.c_str());
    EXPECT_EQ(row.str, mode.as_string());
    EXPECT_EQ(row.bits, uint16_t(mode));
    if (row.valid)
      EXPECT_TRUE(mode.valid());
    else
      EXPECT_FALSE(mode.valid());
  }
}
