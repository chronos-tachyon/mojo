// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <cerrno>

#include "base/result.h"

using base::Result;
using RC = base::ResultCode;

TEST(Result, Basics) {
  Result result;
  EXPECT_EQ(RC::OK, result.code());
  EXPECT_EQ(0, result.errno_value());
  EXPECT_EQ("", result.message());
  EXPECT_EQ("OK(0)", result.as_string());

  result = Result::eof("foo", 123);
  EXPECT_EQ(RC::END_OF_FILE, result.code());
  EXPECT_EQ(-1, result.errno_value());
  EXPECT_EQ("foo123", result.message());
  EXPECT_EQ("END_OF_FILE(18): foo123", result.as_string());

  result = Result::from_errno(EEXIST, "mkdir(2)");
  EXPECT_EQ(RC::ALREADY_EXISTS, result.code());
  EXPECT_EQ(EEXIST, result.errno_value());
  EXPECT_EQ("mkdir(2)", result.message());
  EXPECT_EQ("ALREADY_EXISTS(6): mkdir(2) errno:[EEXIST File exists]",
            result.as_string());
}
