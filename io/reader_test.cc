// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <condition_variable>
#include <mutex>
#include <thread>

#include "base/cleanup.h"
#include "base/fd.h"
#include "base/logging.h"
#include "base/result_testing.h"
#include "event/task.h"
#include "io/reader.h"
#include "io/writer.h"
#include "io/util.h"

// StringReader {{{

TEST(StringReader, ZeroThree) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::stringreader("abcdef");

  r.read(&task, buf, &len, 0, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(3U, len);
  EXPECT_EQ("abc", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 0, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(3U, len);
  EXPECT_EQ("def", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 0, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(0U, len);
}

TEST(StringReader, OneThree) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::stringreader("abcdef");

  r.read(&task, buf, &len, 1, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(3U, len);
  EXPECT_EQ("abc", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(3U, len);
  EXPECT_EQ("def", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 3);
  EXPECT_EOF(task.result());
  EXPECT_EQ(0U, len);
}

TEST(StringReader, ZeroFour) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::stringreader("abcdef");

  r.read(&task, buf, &len, 0, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, len);
  EXPECT_EQ("abcd", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 0, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(2U, len);
  EXPECT_EQ("ef", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 0, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(0U, len);
}

TEST(StringReader, OneFour) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::stringreader("abcdef");

  r.read(&task, buf, &len, 1, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, len);
  EXPECT_EQ("abcd", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(2U, len);
  EXPECT_EQ("ef", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 4);
  EXPECT_EOF(task.result());
  EXPECT_EQ(0U, len);
}

TEST(StringReader, ThreeFour) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::stringreader("abcdef");

  r.read(&task, buf, &len, 3, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, len);
  EXPECT_EQ("abcd", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 3, 4);
  EXPECT_EOF(task.result());
  EXPECT_EQ(2U, len);
  EXPECT_EQ("ef", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 3, 4);
  EXPECT_EOF(task.result());
  EXPECT_EQ(0U, len);
}

TEST(StringReader, WriteTo) {
  io::Reader r;
  io::Writer w;
  event::Task task;
  char buf[16];
  std::size_t len = 0;
  std::size_t copied = 42;

  r = io::stringreader("abcdefg");
  w = io::bufferwriter(buf, sizeof(buf), &len);
  r.write_to(&task, &copied, ~std::size_t(0), w);
  EXPECT_OK(task.result());
  EXPECT_EQ(7U, copied);
  EXPECT_EQ(7U, len);
  EXPECT_EQ("abcdefg", std::string(buf, len));
}

TEST(StringReader, Close) {
  event::Task task;
  io::Reader r = io::stringreader("");
  r.close(&task);
  EXPECT_OK(task.result());
  task.reset();
  r.close(&task);
  EXPECT_FAILED_PRECONDITION(task.result());
}

// }}}
// BufferReader {{{

TEST(BufferReader, ZeroThree) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::bufferreader("abcdef", 6);

  r.read(&task, buf, &len, 0, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(3U, len);
  EXPECT_EQ("abc", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 0, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(3U, len);
  EXPECT_EQ("def", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 0, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(0U, len);
}

TEST(BufferReader, OneThree) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::bufferreader("abcdef", 6);

  r.read(&task, buf, &len, 1, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(3U, len);
  EXPECT_EQ("abc", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(3U, len);
  EXPECT_EQ("def", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 3);
  EXPECT_EOF(task.result());
  EXPECT_EQ(0U, len);
}

TEST(BufferReader, ZeroFour) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::bufferreader("abcdef", 6);

  r.read(&task, buf, &len, 0, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, len);
  EXPECT_EQ("abcd", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 0, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(2U, len);
  EXPECT_EQ("ef", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 0, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(0U, len);
}

TEST(BufferReader, OneFour) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::bufferreader("abcdef", 6);

  r.read(&task, buf, &len, 1, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, len);
  EXPECT_EQ("abcd", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(2U, len);
  EXPECT_EQ("ef", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 4);
  EXPECT_EOF(task.result());
  EXPECT_EQ(0U, len);
}

TEST(BufferReader, ThreeFour) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::bufferreader("abcdef", 6);

  r.read(&task, buf, &len, 3, 4);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, len);
  EXPECT_EQ("abcd", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 3, 4);
  EXPECT_EOF(task.result());
  EXPECT_EQ(2U, len);
  EXPECT_EQ("ef", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 3, 4);
  EXPECT_EOF(task.result());
  EXPECT_EQ(0U, len);
}

TEST(BufferReader, WriteTo) {
  io::Reader r;
  io::Writer w;
  event::Task task;
  char buf[16];
  std::size_t len = 0;
  std::size_t copied = 42;

  r = io::bufferreader("abcdefg", 7);
  w = io::bufferwriter(buf, sizeof(buf), &len);
  r.write_to(&task, &copied, ~std::size_t(0), w);
  EXPECT_OK(task.result());
  EXPECT_EQ(7U, copied);
  EXPECT_EQ(7U, len);
  EXPECT_EQ("abcdefg", std::string(buf, len));
}

TEST(BufferReader, Close) {
  event::Task task;
  io::Reader r = io::bufferreader("", 0);
  r.close(&task);
  EXPECT_OK(task.result());
  task.reset();
  r.close(&task);
  EXPECT_FAILED_PRECONDITION(task.result());
}

// }}}
// IgnoreCloseReader {{{

TEST(IgnoreCloseReader, Close) {
  int n = 0;
  auto rfn = [](event::Task* task, char* ptr, std::size_t* len, std::size_t min,
                std::size_t max) {
    *len = 0;
    if (task->start()) task->finish(base::Result::not_implemented());
  };
  auto cfn = [&n](event::Task* task) {
    ++n;
    if (task->start()) task->finish_ok();
  };

  io::Reader r;
  event::Task task;

  r = io::reader(rfn, cfn);

  r.close(&task);
  EXPECT_OK(task.result());
  EXPECT_EQ(1, n);

  task.reset();
  r.close(&task);
  EXPECT_OK(task.result());
  EXPECT_EQ(2, n);

  r = io::ignore_close(std::move(r));

  task.reset();
  r.close(&task);
  EXPECT_OK(task.result());
  EXPECT_EQ(2, n);
}

// }}}
// LimitedReader {{{

TEST(LimitedReader, BigRead) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::limited_reader(io::stringreader("abcdef"), 4);

  r.read(&task, buf, &len, 1, 16);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, len);
  EXPECT_EQ("abcd", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 16);
  EXPECT_EOF(task.result());
  EXPECT_EQ(0U, len);
}

TEST(LimitedReader, SmallReadAligned) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::limited_reader(io::stringreader("abcdef"), 4);

  r.read(&task, buf, &len, 1, 2);
  EXPECT_OK(task.result());
  EXPECT_EQ(2U, len);
  EXPECT_EQ("ab", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 2);
  EXPECT_OK(task.result());
  EXPECT_EQ(2U, len);
  EXPECT_EQ("cd", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 2);
  EXPECT_EOF(task.result());
  EXPECT_EQ(0U, len);
}

TEST(LimitedReader, SmallReadUnaligned) {
  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t len;

  r = io::limited_reader(io::stringreader("abcdef"), 4);

  r.read(&task, buf, &len, 1, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(3U, len);
  EXPECT_EQ("abc", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 3);
  EXPECT_OK(task.result());
  EXPECT_EQ(1U, len);
  EXPECT_EQ("d", std::string(buf, len));

  task.reset();
  r.read(&task, buf, &len, 1, 3);
  EXPECT_EOF(task.result());
  EXPECT_EQ(0U, len);
}

TEST(LimitedReader, WriteTo) {
  io::Reader r;
  io::Writer w;
  event::Task task;
  std::string in(8192, 'a');
  std::string out;
  std::size_t n = 0;

  r = io::limited_reader(io::stringreader(in), 4096);
  w = io::stringwriter(&out);

  r.write_to(&task, &n, 4096, w);
  EXPECT_OK(task.result());
  EXPECT_EQ(4096U, n);
  EXPECT_EQ(in.substr(0, out.size()), out);

  task.reset();
  r.write_to(&task, &n, 4096, w);
  EXPECT_OK(task.result());
  EXPECT_EQ(0U, n);
  EXPECT_EQ(in.substr(0, out.size()), out);

  out.clear();
  r = io::limited_reader(io::stringreader(in), 4096);
  w = io::stringwriter(&out);

  task.reset();
  r.write_to(&task, &n, 3072, w);
  EXPECT_OK(task.result());
  EXPECT_EQ(3072U, n);
  EXPECT_EQ(in.substr(0, out.size()), out);

  task.reset();
  r.write_to(&task, &n, 3072, w);
  EXPECT_OK(task.result());
  EXPECT_EQ(1024U, n);
  EXPECT_EQ(in.substr(0, out.size()), out);

  task.reset();
  r.write_to(&task, &n, 3072, w);
  EXPECT_OK(task.result());
  EXPECT_EQ(0U, n);
  EXPECT_EQ(in.substr(0, out.size()), out);
}

// }}}
// NullReader {{{

TEST(NullReader, Read) {
  io::Reader r = io::nullreader();

  event::Task task;
  char buf[16];
  std::size_t n = 42;

  r.read(&task, buf, &n, 0, sizeof(buf));
  event::wait(r.manager(), &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(0U, n);

  task.reset();
  n = 42;
  r.read(&task, buf, &n, 1, sizeof(buf));
  event::wait(r.manager(), &task);
  EXPECT_EOF(task.result());
  EXPECT_EQ(0U, n);
}

// }}}
// ZeroReader {{{

TEST(ZeroReader, Read) {
  io::Reader r = io::zeroreader();

  event::Task task;
  char buf[16];
  std::size_t n = 42;
  std::string expected;
  expected.assign(sizeof(buf), '\0');

  r.read(&task, buf, &n, 0, sizeof(buf));
  event::wait(r.manager(), &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(sizeof(buf), n);
  EXPECT_EQ(expected, std::string(buf, n));

  task.reset();
  n = 42;
  r.read(&task, buf, &n, 1, sizeof(buf));
  event::wait(r.manager(), &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(sizeof(buf), n);
  EXPECT_EQ(expected, std::string(buf, n));

  task.reset();
  n = 42;
  r.read(&task, buf, &n, sizeof(buf), sizeof(buf));
  event::wait(r.manager(), &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(sizeof(buf), n);
  EXPECT_EQ(expected, std::string(buf, n));
}

// }}}
// FDReader {{{

static void TestFDReader_Read(event::ManagerOptions mo) {
  base::Pipe pipe;
  ASSERT_OK(base::make_pipe(&pipe));

  LOG(INFO) << "made pipes";

  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));
  ASSERT_TRUE(m);

  LOG(INFO) << "made manager";

  std::mutex mu;
  std::condition_variable cv;
  std::size_t x = 0, y = 0;

  std::thread t([&pipe, &mu, &cv, &x, &y] {
    std::unique_lock<std::mutex> lock(mu);

    while (x < 1) cv.wait(lock);
    EXPECT_OK(base::write_exactly(pipe.write, "abcd", 4, "pipe"));
    LOG(INFO) << "wrote: abcd";

    while (x < 2) cv.wait(lock);
    EXPECT_OK(base::write_exactly(pipe.write, "efgh", 4, "pipe"));
    LOG(INFO) << "wrote: efgh";

    ++y;
    cv.notify_all();

    while (x < 3) cv.wait(lock);
    EXPECT_OK(base::write_exactly(pipe.write, "ijkl", 4, "pipe"));
    LOG(INFO) << "wrote: ijkl";
  });

  LOG(INFO) << "spawned thread";

  io::Reader r;
  event::Task task;
  char buf[16];
  std::size_t n;

  io::Options o;
  o.set_manager(m);
  r = io::fdreader(pipe.read, o);

  LOG(INFO) << "made fdreader";

  r.read(&task, buf, &n, 0, sizeof(buf));
  event::wait(m, &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(0U, n);
  LOG(INFO) << "read zero bytes";

  std::unique_lock<std::mutex> lock(mu);
  ++x;
  cv.notify_all();
  LOG(INFO) << "woke thread";
  lock.unlock();

  task.reset();
  r.read(&task, buf, &n, 1, sizeof(buf));
  event::wait(m, &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, n);
  EXPECT_EQ("abcd", std::string(buf, n));
  LOG(INFO) << "read four bytes";

  lock.lock();
  ++x;
  cv.notify_all();
  LOG(INFO) << "woke thread";
  while (y < 1) cv.wait(lock);
  lock.unlock();

  task.reset();
  r.read(&task, buf, &n, 8, sizeof(buf));
  LOG(INFO) << "initiated read";

  lock.lock();
  ++x;
  cv.notify_all();
  LOG(INFO) << "woke thread";
  lock.unlock();

  event::wait(m, &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(8U, n);
  EXPECT_EQ("efghijkl", std::string(buf, n));
  LOG(INFO) << "read eight bytes";

  t.join();
}

static void TestFDReader_WriteTo(event::ManagerOptions mo, io::Options o) {
  std::string path;
  base::FD fd;

  ASSERT_OK(base::make_tempfile(&path, &fd, "mojo-io-reader-test.XXXXXX"));
  auto cleanup0 = base::cleanup([path] { ::unlink(path.c_str()); });

  std::string tmp;
  for (std::size_t i = 0; i < 16; ++i) {
    tmp.assign(4096, 'A' + i);
    ASSERT_OK(base::write_exactly(fd, tmp.data(), tmp.size(), "temp file"));
  }
  ASSERT_OK(base::seek(nullptr, fd, 0, SEEK_SET));

  LOG(INFO) << "temp file is ready";

  base::SocketPair s;
  ASSERT_OK(base::make_socketpair(&s, AF_UNIX, SOCK_STREAM, 0));
  ASSERT_OK(base::set_blocking(s.right, true));

  LOG(INFO) << "socketpair is ready";

  std::mutex mu;
  std::condition_variable cv;
  bool ready = false;
  std::size_t sunk = 0;

  std::thread t([&s, &mu, &cv, &ready, &sunk] {
    base::Result r;
    std::vector<char> buf;
    std::string expected;
    std::size_t i = 0;

    buf.resize(4096);
    expected.resize(4096);

    auto lock = base::acquire_lock(mu);
    while (!ready) cv.wait(lock);

    LOG(INFO) << "sink thread running";
    while (true) {
      expected.assign(4096, 'A' + i);
      r = base::read_exactly(s.right, buf.data(), buf.size(), "socket");
      if (!r) break;
      EXPECT_EQ(expected, std::string(buf.data(), buf.size()));
      sunk += buf.size();
      ++i;
    }
    EXPECT_EOF(r);
  });

  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));
  ASSERT_TRUE(m);

  LOG(INFO) << "made manager";

  o.set_manager(m);

  io::Reader r = io::fdreader(fd, o);
  io::Writer w = io::fdwriter(s.left, o);

  event::Task task;
  std::size_t n;
  io::copy(&task, &n, w, r);

  auto lock = base::acquire_lock(mu);
  ready = true;
  cv.notify_all();
  lock.unlock();

  event::wait(m, &task);
  LOG(INFO) << "task done";
  EXPECT_OK(task.result());
  EXPECT_EQ(16U * 4096U, n);

  ASSERT_OK(base::shutdown(s.left, SHUT_WR));
  t.join();
  LOG(INFO) << "thread done";
  EXPECT_EQ(sunk, n);
}

TEST(FDReader, InlineRead) {
  event::ManagerOptions mo;
  mo.set_num_pollers(0, 1);
  mo.dispatcher().set_type(event::DispatcherType::inline_dispatcher);
  TestFDReader_Read(std::move(mo));
}

TEST(FDReader, AsyncRead) {
  event::ManagerOptions mo;
  mo.set_num_pollers(0, 1);
  mo.dispatcher().set_type(event::DispatcherType::async_dispatcher);
  TestFDReader_Read(std::move(mo));
}

TEST(FDReader, ThreadedRead) {
  event::ManagerOptions mo;
  mo.set_num_pollers(1);
  mo.dispatcher().set_type(event::DispatcherType::threaded_dispatcher);
  mo.dispatcher().set_num_workers(1);
  TestFDReader_Read(std::move(mo));
}

TEST(FDReader, WriteToFallback) {
  event::ManagerOptions mo;
  mo.set_num_pollers(0, 1);
  mo.dispatcher().set_type(event::DispatcherType::async_dispatcher);
  io::Options o;
  o.set_transfer_mode(io::TransferMode::read_write);
  TestFDReader_WriteTo(std::move(mo), std::move(o));
}

TEST(FDReader, WriteToSendfile) {
  event::ManagerOptions mo;
  mo.set_num_pollers(0, 1);
  mo.dispatcher().set_type(event::DispatcherType::async_dispatcher);
  io::Options o;
  o.set_transfer_mode(io::TransferMode::sendfile);
  TestFDReader_WriteTo(std::move(mo), std::move(o));
}

TEST(FDReader, WriteToSplice) {
  event::ManagerOptions mo;
  mo.set_num_pollers(0, 1);
  mo.dispatcher().set_type(event::DispatcherType::async_dispatcher);
  io::Options o;
  o.set_transfer_mode(io::TransferMode::splice);
  TestFDReader_WriteTo(std::move(mo), std::move(o));
}

// }}}

static void init() __attribute__((constructor));
static void init() { base::log_stderr_set_level(VLOG_LEVEL(6)); }
