// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <fcntl.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <unistd.h>

#include <condition_variable>
#include <mutex>
#include <thread>

#include "base/cleanup.h"
#include "base/logging.h"
#include "base/result_testing.h"
#include "io/reader.h"
#include "io/writer.h"

static constexpr char kHex[] = "0123456789abcdef";

static std::string show(const std::vector<char>& vec, std::size_t idx) {
  std::string out;
  out.push_back('[');
  std::size_t i = 0;
  if (idx >= 5) {
    out.append("... ");
    i = idx - 3;
  }
  std::size_t j = vec.size();
  bool abbrev_end = false;
  if (vec.size() - idx >= 5) {
    abbrev_end = true;
    j = idx + 3;
  }
  while (i < j) {
    unsigned char ch = vec[i];
    out.push_back(kHex[(ch >> 4) & 0xfU]);
    out.push_back(kHex[ch & 0xfU]);
    out.push_back(' ');
    ++i;
  }
  if (abbrev_end) {
    out.append("...");
  }
  out.push_back(']');
  return out;
}

static testing::AssertionResult equalvec(const char* aexpr, const char* bexpr,
                                         const std::vector<char>& a,
                                         const std::vector<char>& b) {
  if (a.size() != b.size()) {
    return testing::AssertionFailure()
           << "lengths differ\n"
           << "expected: " << aexpr << " (" << a.size() << " bytes)\n"
           << "  actual: " << bexpr << " (" << b.size() << " bytes)";
  }
  for (std::size_t i = 0, n = a.size(); i < n; ++i) {
    if (a[i] != b[i]) {
      return testing::AssertionFailure()
             << "vectors differ\n"
             << "expected: " << aexpr << " " << show(a, i) << "\n"
             << "  actual: " << bexpr << " " << show(b, i);
    }
  }
  return testing::AssertionSuccess();
}

// StringWriter {{{

TEST(StringWriter, Write) {
  io::Writer w;
  event::Task task;
  std::string out;

  w = io::stringwriter(&out);
  std::size_t n = 42;

  w.write(&task, &n, "abc", 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(3U, n);
  EXPECT_EQ("abc", out);

  task.reset();
  w.write(&task, &n, "defg", 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, n);
  EXPECT_EQ("abcdefg", out);
}

TEST(StringWriter, ReadFrom) {
  io::Reader r;
  io::Writer w;
  std::string out;
  event::Task task;
  std::size_t copied = 42;

  r = io::bufferreader("abcdefg", 7);
  w = io::stringwriter(&out);
  w.read_from(&task, &copied, 16, r);
  event::wait(event::system_manager(), &task);
  EXPECT_NOT_IMPLEMENTED(task.result());
  EXPECT_EQ(0U, copied);
}

TEST(StringWriter, Close) {
  event::Task task;
  std::string out;
  io::Writer w = io::stringwriter(&out);
  EXPECT_OK(w.close());
  EXPECT_FAILED_PRECONDITION(w.close());
}

// }}}
// BufferWriter {{{

TEST(BufferWriter, Write) {
  io::Writer w;
  event::Task task;
  char buf[16];
  std::size_t len = 9001;
  std::size_t n = 42;

  w = io::bufferwriter(buf, sizeof(buf), &len);
  EXPECT_EQ(0U, len);

  w.write(&task, &n, "abc", 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(3U, n);
  EXPECT_EQ(3U, len);
  EXPECT_EQ("abc", std::string(buf, len));

  task.reset();
  w.write(&task, &n, "defg", 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, n);
  EXPECT_EQ(7U, len);
  EXPECT_EQ("abcdefg", std::string(buf, len));
}

TEST(BufferWriter, ReadFrom) {
  io::Reader r;
  io::Writer w;
  event::Task task;
  char buf[16];
  std::size_t len = 0;
  std::size_t copied = 42;

  r = io::bufferreader("abcdefg", 7);
  w = io::bufferwriter(buf, sizeof(buf), &len);
  w.read_from(&task, &copied, sizeof(buf), r);
  EXPECT_OK(task.result());
  EXPECT_EQ(7U, copied);
  EXPECT_EQ(7U, len);
  EXPECT_EQ("abcdefg", std::string(buf, len));

  r = io::bufferreader("abcdefg", 7);
  w = io::bufferwriter(buf, sizeof(buf), &len);
  task.reset();
  w.read_from(&task, &copied, 4, r);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, copied);
  EXPECT_EQ(4U, len);
  EXPECT_EQ("abcd", std::string(buf, len));
}

TEST(BufferWriter, Close) {
  event::Task task;
  std::size_t len = 0;
  io::Writer w = io::bufferwriter(nullptr, 0, &len);
  EXPECT_OK(w.close());
  EXPECT_FAILED_PRECONDITION(w.close());
}

// }}}
// IgnoreCloseWriter {{{

TEST(IgnoreCloseWriter, Close) {
  int n = 0;
  auto wfn = [](event::Task* task, std::size_t* copied, const char* ptr,
                std::size_t len, const base::Options& opts) {
    *copied = 0;
    if (task->start()) task->finish(base::Result::not_implemented());
  };
  auto cfn = [&n](event::Task* task, const base::Options& opts) {
    ++n;
    if (task->start()) task->finish_ok();
  };

  io::Writer w;

  w = io::writer(wfn, cfn);

  EXPECT_OK(w.close());
  EXPECT_EQ(1, n);

  EXPECT_OK(w.close());
  EXPECT_EQ(2, n);

  w = io::ignore_close(w);

  EXPECT_OK(w.close());
  EXPECT_EQ(2, n);
}

// }}}
// DiscardWriter {{{

TEST(DiscardWriter, Write) {
  std::size_t total = 42;
  io::Writer w = io::discardwriter(&total);
  EXPECT_EQ(0U, total);

  event::Manager m = event::system_manager();

  event::Task task;
  std::size_t n = 42;

  w.write(&task, &n, "abcdefgh", 8);
  event::wait(m, &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(8U, n);
  EXPECT_EQ(8U, total);

  task.reset();
  w.write(&task, &n, "ijkl", 4);
  event::wait(m, &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, n);
  EXPECT_EQ(12U, total);

  w = io::discardwriter();
  total = 0;

  task.reset();
  w.write(&task, &n, "abcdefgh", 8);
  event::wait(m, &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(8U, n);
  EXPECT_EQ(0U, total);

  task.reset();
  w.write(&task, &n, "ijkl", 4);
  event::wait(m, &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, n);
  EXPECT_EQ(0U, total);
}

// }}}
// FullWriter {{{

TEST(FullWriter, Write) {
  io::Writer w = io::fullwriter();

  event::Manager m = event::system_manager();

  event::Task task;
  std::size_t n = 42;

  w.write(&task, &n, "", 0);
  event::wait(m, &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(0U, n);

  task.reset();
  w.write(&task, &n, "a", 1);
  event::wait(m, &task);
  EXPECT_RESOURCE_EXHAUSTED(task.result());
  EXPECT_EQ(ENOSPC, task.result().errno_value());
  EXPECT_EQ(0U, n);
}

// }}}
// FDWriter {{{

static void FDWriterTest(event::ManagerOptions mo) {
  base::Pipe pipe;
  ASSERT_OK(base::make_pipe(&pipe));

  {
    auto pair = pipe.write->acquire_fd();
    ::fcntl(pair.first, F_SETPIPE_SZ, 4096);
  }

  LOG(INFO) << "made pipes";

  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));
  ASSERT_TRUE(m);

  base::Options o;
  o.get<io::Options>().manager = m;

  LOG(INFO) << "made manager";

  struct sigaction sa;
  ::bzero(&sa, sizeof(sa));
  sa.sa_handler = SIG_IGN;
  ::sigaction(SIGPIPE, &sa, nullptr);

  std::string expected;
  std::vector<char> buf;
  char ch = 'A';
  buf.assign(1024, ch);
  std::size_t wrote = 0;
  while (true) {
    auto pair = pipe.write->acquire_fd();
    ssize_t n = ::write(pair.first, buf.data(), buf.size());
    if (n < 0) {
      int err_no = errno;
      if (err_no == EINTR) continue;
      if (err_no == EPIPE || err_no == EAGAIN || err_no == EWOULDBLOCK) break;
      auto result = base::Result::from_errno(err_no, "write(2)");
      EXPECT_OK(result);
      break;
    }
    wrote += n;
    expected.append(buf.data(), n);
    buf.assign(1024, ++ch);
  }
  EXPECT_GE(wrote, 1024U);

  LOG(INFO) << "filled pipe with " << expected.size() << " bytes";

  std::mutex mu;
  std::condition_variable cv;
  bool ready = false;
  bool done = false;
  std::string out;

  std::thread t([&pipe, &mu, &cv, &ready, &done, &out] {
    std::unique_lock<std::mutex> lock(mu);
    while (!ready) cv.wait(lock);
    LOG(INFO) << "read thread running";
    std::vector<char> buf;
    buf.resize(256);
    while (true) {
      auto pair = pipe.read->acquire_fd();
      int n = ::read(pair.first, buf.data(), buf.size());
      int err_no = errno;
      pair.second.unlock();
      if (n < 0) {
        if (err_no == EINTR) continue;
        if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
          using MS = std::chrono::milliseconds;
          std::this_thread::sleep_for(MS(1));
          continue;
        }
        auto result = base::Result::from_errno(err_no, "read(2)");
        EXPECT_OK(result);
        break;
      }
      LOG(INFO) << "read " << n << " bytes";
      if (n == 0) break;
      out.append(buf.data(), n);
    }
    done = true;
    cv.notify_all();
  });
  auto cleanup1 = base::cleanup([&t] { t.join(); });

  LOG(INFO) << "spawned thread";

  io::Writer w;
  event::Task task;
  std::size_t n;

  w = io::fdwriter(pipe.write);

  LOG(INFO) << "created fdwriter";

  buf.assign(1024, ++ch);
  w.write(&task, &n, buf.data(), buf.size(), o);

  LOG(INFO) << "started write";

  std::unique_lock<std::mutex> lock(mu);
  ready = true;
  cv.notify_all();
  lock.unlock();

  LOG(INFO) << "unblocked reads";

  event::wait(m, &task);
  expected.append(buf.data(), n);
  EXPECT_OK(task.result());
  EXPECT_EQ(1024U, n);

  LOG(INFO) << "wrote additional data";

  EXPECT_OK(w.close(o));

  LOG(INFO) << "closed pipe";

  lock.lock();
  while (!done) cv.wait(lock);
  EXPECT_EQ(expected, out);

  base::log_flush();
}

TEST(FDWriter, AsyncWrite) {
  event::ManagerOptions mo;
  mo.set_async_mode();
  FDWriterTest(std::move(mo));
}

TEST(FDWriter, ThreadedWrite) {
  event::ManagerOptions mo;
  mo.set_minimal_threaded_mode();
  FDWriterTest(std::move(mo));
}

// }}}
// BufferedWriter {{{

void TestBufferedWriter(const event::ManagerOptions& mo, const char* what) {
  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options o;
  o.get<io::Options>().manager = m;

  std::string path;
  base::FD fd;
  ASSERT_OK(base::make_tempfile(&path, &fd,
                                "mojo_io_writer_TestBufferedWriter_XXXXXXXX"));
  auto cleanup = base::cleanup([&path] { ::unlink(path.c_str()); });

  LOG(INFO) << "[TestBufferedWriter:" << what << ":begin]";

  io::Writer w = io::bufferedwriter(io::fdwriter(fd));

  w.write_u8(0x00U, o);
  w.write_u8(0x7fU, o);
  w.write_u8(0x80U, o);
  w.write_u8(0xffU, o);
  w.write_u16(0x0000U, base::kBigEndian, o);
  w.write_u16(0x7fffU, base::kBigEndian, o);
  w.write_u16(0x8000U, base::kBigEndian, o);
  w.write_u16(0xffffU, base::kBigEndian, o);
  w.write_u32(0x00000000U, base::kBigEndian, o);
  w.write_u32(0x7fffffffU, base::kBigEndian, o);
  w.write_u32(0x80000000U, base::kBigEndian, o);
  w.write_u32(0xffffffffU, base::kBigEndian, o);
  w.write_u64(0x0000000000000000ULL, base::kBigEndian, o);
  w.write_u64(0x7fffffffffffffffULL, base::kBigEndian, o);
  w.write_u64(0x8000000000000000ULL, base::kBigEndian, o);
  w.write_u64(0xffffffffffffffffULL, base::kBigEndian, o);

  w.write_s8(0x01, o);
  w.write_s8(0x7f, o);
  w.write_s8(-0x7f, o);
  w.write_s8(-0x01, o);
  w.write_s16(0x0001, base::kBigEndian, o);
  w.write_s16(0x7fff, base::kBigEndian, o);
  w.write_s16(-0x7fff, base::kBigEndian, o);
  w.write_s16(-0x0001, base::kBigEndian, o);
  w.write_s32(0x00000001, base::kBigEndian, o);
  w.write_s32(0x7fffffff, base::kBigEndian, o);
  w.write_s32(-0x7fffffff, base::kBigEndian, o);
  w.write_s32(-0x00000001, base::kBigEndian, o);
  w.write_s64(0x0000000000000001LL, base::kBigEndian, o);
  w.write_s64(0x7fffffffffffffffLL, base::kBigEndian, o);
  w.write_s64(-0x7fffffffffffffffLL, base::kBigEndian, o);
  w.write_s64(-0x0000000000000001LL, base::kBigEndian, o);

  w.write_uvarint(0, o);
  w.write_uvarint(1, o);
  w.write_uvarint(127, o);
  w.write_uvarint(128, o);
  w.write_uvarint(300, o);
  w.write_uvarint(16383, o);
  w.write_uvarint(65535, o);
  w.write_uvarint(0xffffffffffffffffULL, o);

  w.write_svarint(0, o);
  w.write_svarint(1, o);
  w.write_svarint(127, o);
  w.write_svarint(128, o);
  w.write_svarint(300, o);
  w.write_svarint(-1, o);

  w.write_svarint_zigzag(0, o);
  w.write_svarint_zigzag(1, o);
  w.write_svarint_zigzag(2, o);
  w.write_svarint_zigzag(150, o);
  w.write_svarint_zigzag(-1, o);
  w.write_svarint_zigzag(-2, o);
  w.write_svarint_zigzag(-150, o);

  w.flush(o);

  std::vector<char> data;
  EXPECT_OK(base::seek(nullptr, fd, 0, SEEK_SET));
  EXPECT_OK(base::read_all(&data, fd, path.c_str()));

  constexpr unsigned char kExpected[] = {
      0x00, 0x7f, 0x80, 0xff, 0x00, 0x00, 0x7f, 0xff, 0x80, 0x00, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00,
      0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0x01, 0x7f, 0x81, 0xff, 0x00, 0x01, 0x7f, 0xff, 0x80, 0x01, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01, 0x7f, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x01,
      0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
      0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0x00, 0x01, 0x7f, 0x80, 0x01, 0xac, 0x02, 0xff, 0x7f, 0xff, 0xff, 0x03,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x01,
      0x7f, 0x80, 0x01, 0xac, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0x01, 0x00, 0x02, 0x04, 0xac, 0x02, 0x01, 0x03, 0xab, 0x02,
  };
  const char* p = reinterpret_cast<const char*>(&kExpected);
  std::vector<char> expected(p, p + sizeof(kExpected));

  EXPECT_PRED_FORMAT2(equalvec, expected, data);

  LOG(INFO) << "[TestBufferedWriter:" << what << ":end]";

  EXPECT_OK(fd->close());
  m.shutdown();
  cleanup.run();

  base::log_flush();
}

TEST(BufferedWriter, Async) {
  event::ManagerOptions mo;
  mo.set_async_mode();
  TestBufferedWriter(mo, "async");
}

TEST(BufferedWriter, Threaded) {
  event::ManagerOptions mo;
  mo.set_threaded_mode();
  mo.set_num_pollers(2);
  mo.dispatcher().set_num_workers(2);
  TestBufferedWriter(mo, "threaded");
}

// }}}

static void init() __attribute__((constructor));
static void init() { base::log_stderr_set_level(VLOG_LEVEL(6)); }
