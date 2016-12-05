// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/debug.h"
#include "base/fd.h"
#include "base/logging.h"
#include "base/result_testing.h"
#include "external/com_googlesource_code_re2/re2/re2.h"

static pid_t my_gettid() { return 42; }

static int my_gettimeofday(struct timeval* tv, struct timezone* unused) {
  // Mon 2006 Jan 02 15:04:05.123456 -0700
  tv->tv_sec = 1136239445;
  tv->tv_usec = 123456;
  return 0;
}

static base::Result setup(base::Pipe* pipe) {
  base::Result r = base::make_pipe(pipe);
  if (r) {
    base::log_set_gettid(my_gettid);
    base::log_set_gettimeofday(my_gettimeofday);
    auto pair = pipe->write->acquire_fd();
    base::log_fd_set_level(pair.first, LOG_LEVEL_INFO);
  }
  return r;
}

static base::Result teardown(std::string* str, base::Pipe& pipe) {
  {
    auto pair = pipe.write->acquire_fd();
    base::log_fd_remove(pair.first);
  }
  base::Result wr = pipe.write->close();
  base::Result r;
  str->clear();
  while (true) {
    char buf[256];
    auto pair = pipe.read->acquire_fd();
    ssize_t n = ::read(pair.first, buf, sizeof(buf));
    if (n < 0) {
      int err_no = errno;
      r = base::Result::from_errno(err_no, "read(2)");
      break;
    }
    if (n == 0) break;
    str->append(buf, n);
  }
  re2::RE2::GlobalReplace(str, ":[0-9]+\\] ", ":XX] ");
  base::Result rr = pipe.read->close();
  if (r) r = wr;
  if (r) r = rr;
  return r;
}

TEST(Logger, EndToEnd) {
  base::Pipe pipe;
  ASSERT_OK(setup(&pipe));

  VLOG(0) << "who cares?";
  LOG(INFO) << "hello";
  LOG(WARN) << "uh oh";
  LOG(ERROR) << "oh no!";
  EXPECT_THROW(LOG(FATAL) << "aaaah!", base::fatal_error);

  std::string data;
  ASSERT_OK(teardown(&data, pipe));

  std::string expected(
      "I0102 22:04:05.123456  42 base/logging_test.cc:XX] hello\n"
      "W0102 22:04:05.123456  42 base/logging_test.cc:XX] uh oh\n"
      "E0102 22:04:05.123456  42 base/logging_test.cc:XX] oh no!\n"
      "F0102 22:04:05.123456  42 base/logging_test.cc:XX] aaaah!\n");
  EXPECT_EQ(expected, data);
}

TEST(Logger, LogEveryN) {
  base::Pipe pipe;
  ASSERT_OK(setup(&pipe));

  for (std::size_t i = 0; i < 10; ++i) {
    LOG_EVERY_N(INFO, 3) << "hi #" << i;
  }

  std::string data;
  ASSERT_OK(teardown(&data, pipe));

  std::string expected(
      "I0102 22:04:05.123456  42 base/logging_test.cc:XX] hi #0\n"
      "I0102 22:04:05.123456  42 base/logging_test.cc:XX] hi #3\n"
      "I0102 22:04:05.123456  42 base/logging_test.cc:XX] hi #6\n"
      "I0102 22:04:05.123456  42 base/logging_test.cc:XX] hi #9\n");
  EXPECT_EQ(expected, data);
}

TEST(Check, Correct) {
  CHECK(true) << ": not used";
  CHECK_NE(0, 1) << ": not used";
  CHECK_LT(2, 3) << ": not used";
  CHECK_LE(2, 3) << ": not used";
  CHECK_LE(3, 3) << ": not used";
  CHECK_EQ(3, 3) << ": not used";
  CHECK_GE(3, 3) << ": not used";
  CHECK_GE(5, 3) << ": not used";
  CHECK_GT(5, 3) << ": not used";
}

TEST(Check, Wrong) {
  base::Pipe pipe;
  ASSERT_OK(setup(&pipe));

  base::set_debug(true);

  const int p = 1, q = 2, r = 3, s = 5;

  EXPECT_THROW(CHECK(false) << ": error #0", base::fatal_error);
  EXPECT_THROW(CHECK_NE(p, p) << ": error #1", base::fatal_error);
  EXPECT_THROW(CHECK_LE(s, r) << ": error #2", base::fatal_error);
  EXPECT_THROW(CHECK_LT(s, r) << ": error #3", base::fatal_error);
  EXPECT_THROW(CHECK_LT(r, r) << ": error #4", base::fatal_error);
  EXPECT_THROW(CHECK_EQ(p, r) << ": error #5", base::fatal_error);
  EXPECT_THROW(CHECK_GT(r, r) << ": error #6", base::fatal_error);
  EXPECT_THROW(CHECK_GT(q, r) << ": error #7", base::fatal_error);
  EXPECT_THROW(CHECK_GE(q, r) << ": error #8", base::fatal_error);

  std::string data;
  ASSERT_OK(teardown(&data, pipe));

  std::string expected(
      "F0102 22:04:05.123456  42 base/logging_test.cc:XX] "
      "CHECK FAILED: false: error #0\n"
      "F0102 22:04:05.123456  42 base/logging_test.cc:XX] "
      "CHECK FAILED: p != p [1 != 1]: error #1\n"
      "F0102 22:04:05.123456  42 base/logging_test.cc:XX] "
      "CHECK FAILED: s <= r [5 <= 3]: error #2\n"
      "F0102 22:04:05.123456  42 base/logging_test.cc:XX] "
      "CHECK FAILED: s < r [5 < 3]: error #3\n"
      "F0102 22:04:05.123456  42 base/logging_test.cc:XX] "
      "CHECK FAILED: r < r [3 < 3]: error #4\n"
      "F0102 22:04:05.123456  42 base/logging_test.cc:XX] "
      "CHECK FAILED: p == r [1 == 3]: error #5\n"
      "F0102 22:04:05.123456  42 base/logging_test.cc:XX] "
      "CHECK FAILED: r > r [3 > 3]: error #6\n"
      "F0102 22:04:05.123456  42 base/logging_test.cc:XX] "
      "CHECK FAILED: q > r [2 > 3]: error #7\n"
      "F0102 22:04:05.123456  42 base/logging_test.cc:XX] "
      "CHECK FAILED: q >= r [2 >= 3]: error #8\n");
  EXPECT_EQ(expected, data);
}

TEST(Check, WrongNDEBUG) {
  base::set_debug(false);

  const int p = 1, q = 2, r = 3, s = 5;
  EXPECT_NO_THROW(CHECK(false) << ": error #0");
  EXPECT_NO_THROW(CHECK_NE(p, p) << ": error #1");
  EXPECT_NO_THROW(CHECK_LE(s, r) << ": error #2");
  EXPECT_NO_THROW(CHECK_LT(s, r) << ": error #3");
  EXPECT_NO_THROW(CHECK_LT(r, r) << ": error #4");
  EXPECT_NO_THROW(CHECK_EQ(p, r) << ": error #5");
  EXPECT_NO_THROW(CHECK_GT(r, r) << ": error #6");
  EXPECT_NO_THROW(CHECK_GT(q, r) << ": error #7");
  EXPECT_NO_THROW(CHECK_GE(q, r) << ": error #8");
}
