// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/result_testing.h"

TEST(ResultTesting, EQ) {
  EXPECT_OK(base::Result());
  EXPECT_UNKNOWN(base::Result::unknown());
  EXPECT_INTERNAL(base::Result::internal());
  EXPECT_CANCELLED(base::Result::cancelled());
  EXPECT_FAILED_PRECONDITION(base::Result::failed_precondition());
  EXPECT_NOT_FOUND(base::Result::not_found());
  EXPECT_ALREADY_EXISTS(base::Result::already_exists());
  EXPECT_WRONG_TYPE(base::Result::wrong_type());
  EXPECT_PERMISSION_DENIED(base::Result::permission_denied());
  EXPECT_UNAUTHENTICATED(base::Result::unauthenticated());
  EXPECT_INVALID_ARGUMENT(base::Result::invalid_argument());
  EXPECT_OUT_OF_RANGE(base::Result::out_of_range());
  EXPECT_NOT_IMPLEMENTED(base::Result::not_implemented());
  EXPECT_UNAVAILABLE(base::Result::unavailable());
  EXPECT_ABORTED(base::Result::aborted());
  EXPECT_RESOURCE_EXHAUSTED(base::Result::resource_exhausted());
  EXPECT_DEADLINE_EXCEEDED(base::Result::deadline_exceeded());
  EXPECT_DATA_LOSS(base::Result::data_loss());
  EXPECT_EOF(base::Result::eof());
}

TEST(ResultTesting, AssertionMessage) {
  bool ok;
  std::string message;
  base::Result result;

  result = base::Result::internal("foo");
  auto x = base::testing::ResultCodeEQ("<ignored>", "<expr>",
                                       base::ResultCode::OK, result);
  ok = x;
  message = x.message();
  EXPECT_FALSE(ok);
  EXPECT_EQ(
      "expression: <expr>\n"
      "  expected: OK(0)\n"
      "       got: INTERNAL(2): foo",
      message);

  x = base::testing::ResultCodeEQ("<ignored>", "<expr>",
                                  base::ResultCode::INTERNAL, result);
  ok = x;
  message = x.message();
  EXPECT_TRUE(ok);
  EXPECT_EQ("", message);
}
