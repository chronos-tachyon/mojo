// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "encoding/hex.h"
#include "gtest/gtest.h"

TEST(Hex, Encode) {
  EXPECT_EQ("", encode(encoding::HEX, ""));
  EXPECT_EQ("6162633132330a", encode(encoding::HEX, "abc123\n"));
}

static std::pair<bool, std::string> boolstr(bool b, std::string s) {
  return std::make_pair(b, std::move(s));
}

TEST(Hex, Decode) {
  EXPECT_EQ(boolstr(true, ""), decode(encoding::HEX, ""));
  EXPECT_EQ(boolstr(true, "abc123\n"), decode(encoding::HEX, "6162633132330a"));
  EXPECT_EQ(boolstr(true, "abc123\n"),
            decode(encoding::HEX, "61 62 63 31 32 33 0a"));
  EXPECT_EQ(boolstr(true, "abc123@"),
            decode(encoding::HEX, "61 62 63 31 32 33 4"));
}
