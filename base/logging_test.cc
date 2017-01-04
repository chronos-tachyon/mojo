// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <unistd.h>

#include <vector>

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

namespace {
class LogCapture : public base::LogTarget {
 public:
  explicit LogCapture(std::string* out) noexcept : out_(out) {
    CHECK_OK(base::make_pipe(&pipe_));
  }

  bool want(const char* file, unsigned int line,
            base::level_t level) const override {
    return level >= LOG_LEVEL_INFO;
  }

  void log(const base::LogEntry& entry) override {
    auto str = entry.as_string();
    auto pair = pipe_.write->acquire_fd();
    ::write(pair.first, str.data(), str.size());
  }

  void flush() override {}

  void finish() {
    CHECK_OK(pipe_.write->close());
    auto pair = pipe_.read->acquire_fd();
    std::vector<char> buf(4096);
    while (true) {
      ssize_t n = ::read(pair.first, buf.data(), buf.size());
      if (n <= 0) break;
      out_->append(buf.data(), n);
    }
  }

  std::string* string() noexcept { return out_; }

 private:
  std::string* const out_;
  base::Pipe pipe_;
};
}  // anonymous namespace

static void setup(LogCapture* target) {
  base::log_set_gettid(my_gettid);
  base::log_set_gettimeofday(my_gettimeofday);
  base::log_target_add(target);
}

static void teardown(LogCapture* target) {
  base::log_target_remove(target);
  target->finish();
  re2::RE2::GlobalReplace(target->string(), ":[0-9]+\\] ", ":XX] ");
}

TEST(LoggerDeathTest, EndToEnd) {
  std::string data;
  LogCapture target(&data);
  setup(&target);

  VLOG(0) << "who cares?";
  LOG(INFO) << "hello";
  LOG(WARN) << "uh oh";
  LOG(ERROR) << "oh no!";
  EXPECT_DEATH(LOG(FATAL) << "aaaah!", "aaaah!");

  teardown(&target);

  std::string expected(
      "I0102 22:04:05.123456  42 base/logging_test.cc:XX] hello\n"
      "W0102 22:04:05.123456  42 base/logging_test.cc:XX] uh oh\n"
      "E0102 22:04:05.123456  42 base/logging_test.cc:XX] oh no!\n"
      "F0102 22:04:05.123456  42 base/logging_test.cc:XX] aaaah!\n");
  EXPECT_EQ(expected, data);
}

TEST(Logger, LogEveryN) {
  std::string data;
  LogCapture target(&data);
  setup(&target);

  for (std::size_t i = 0; i < 10; ++i) {
    LOG_EVERY_N(INFO, 3) << "hi #" << i;
  }

  teardown(&target);

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

TEST(CheckDeathTest, Wrong) {
  std::string data;
  LogCapture target(&data);
  setup(&target);

  base::set_debug(true);

  const int p = 1, q = 2, r = 3, s = 5;

  EXPECT_DEATH(CHECK(false) << ": error #0", "error #0");
  EXPECT_DEATH(CHECK_NE(p, p) << ": error #1", "error #1");
  EXPECT_DEATH(CHECK_LE(s, r) << ": error #2", "error #2");
  EXPECT_DEATH(CHECK_LT(s, r) << ": error #3", "error #3");
  EXPECT_DEATH(CHECK_LT(r, r) << ": error #4", "error #4");
  EXPECT_DEATH(CHECK_EQ(p, r) << ": error #5", "error #5");
  EXPECT_DEATH(CHECK_GT(r, r) << ": error #6", "error #6");
  EXPECT_DEATH(CHECK_GT(q, r) << ": error #7", "error #7");
  EXPECT_DEATH(CHECK_GE(q, r) << ": error #8", "error #8");

  teardown(&target);

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

static void init() __attribute__((constructor));
static void init() { base::log_single_threaded(); }
