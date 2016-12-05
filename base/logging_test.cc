// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <fcntl.h>
#include <unistd.h>

#include <atomic>

#include "base/logging.h"

static pid_t my_gettid() { return 42; }

static int my_gettimeofday(struct timeval* tv, struct timezone* unused) {
  // Mon 2006 Jan 02 15:04:05.123456 -0700
  tv->tv_sec = 1136239445;
  tv->tv_usec = 123456;
  return 0;
}

TEST(Logger, EndToEnd) {
  int fds[2];
  int rc = ::pipe2(fds, O_CLOEXEC);
  ASSERT_EQ(0, rc);

  const int rfd = fds[0];
  const int wfd = fds[1];

  base::log_set_gettid(my_gettid);
  base::log_set_gettimeofday(my_gettimeofday);
  base::log_fd_set_level(wfd, LOG_LEVEL_INFO);

  VLOG(0) << "who cares?";
  LOG(INFO) << "hello";
  LOG(WARN) << "uh oh";
  LOG(ERROR) << "oh no!";
  EXPECT_THROW(LOG(FATAL) << "aaaah!", base::fatal_error);

  base::log_fd_remove(wfd);
  ::close(wfd);

  std::string data;
  while (true) {
    char buf[256];
    int n = ::read(rfd, buf, sizeof(buf));
    if (n < 1) break;
    data.append(buf, n);
  }
  ::close(rfd);

  std::string expected(
      "I0102 22:04:05.123456  42 base/logging_test.cc:37] hello\n"
      "W0102 22:04:05.123456  42 base/logging_test.cc:38] uh oh\n"
      "E0102 22:04:05.123456  42 base/logging_test.cc:39] oh no!\n"
      "F0102 22:04:05.123456  42 base/logging_test.cc:40] aaaah!\n");
  EXPECT_EQ(expected, data);
}

TEST(Logger, LogEveryN) {
  int fds[2];
  int rc = ::pipe2(fds, O_CLOEXEC);
  ASSERT_EQ(0, rc);

  const int rfd = fds[0];
  const int wfd = fds[1];

  base::log_set_gettid(my_gettid);
  base::log_set_gettimeofday(my_gettimeofday);
  base::log_fd_set_level(wfd, LOG_LEVEL_INFO);

  for (std::size_t i = 0; i < 10; ++i) {
    LOG_EVERY_N(INFO, 3) << "hi #" << i;
  }

  base::log_fd_remove(wfd);
  ::close(wfd);

  std::string data;
  while (true) {
    char buf[256];
    int n = ::read(rfd, buf, sizeof(buf));
    if (n < 1) break;
    data.append(buf, n);
  }
  ::close(rfd);

  std::string expected(
      "I0102 22:04:05.123456  42 base/logging_test.cc:75] hi #0\n"
      "I0102 22:04:05.123456  42 base/logging_test.cc:75] hi #3\n"
      "I0102 22:04:05.123456  42 base/logging_test.cc:75] hi #6\n"
      "I0102 22:04:05.123456  42 base/logging_test.cc:75] hi #9\n");
  EXPECT_EQ(expected, data);
}
