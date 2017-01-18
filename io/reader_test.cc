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
#include "io/util.h"
#include "io/writer.h"

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
                std::size_t max, const base::Options& o) {
    *len = 0;
    if (task->start()) task->finish(base::Result::not_implemented());
  };
  auto cfn = [&n](event::Task* task, const base::Options& o) {
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
  base::Options o;

  char buf[16];
  std::size_t n = 42;

  EXPECT_OK(r.read(buf, &n, 0, sizeof(buf), o));
  EXPECT_EQ(0U, n);

  n = 42;
  EXPECT_EOF(r.read(buf, &n, 1, sizeof(buf), o));
  EXPECT_EQ(0U, n);
}

// }}}
// ZeroReader {{{

TEST(ZeroReader, Read) {
  io::Reader r = io::zeroreader();
  base::Options o;

  char buf[16];
  std::size_t n = 42;
  std::string expected;
  expected.assign(sizeof(buf), '\0');

  EXPECT_OK(r.read(buf, &n, 0, sizeof(buf), o));
  EXPECT_EQ(sizeof(buf), n);
  EXPECT_EQ(expected, std::string(buf, n));

  n = 42;
  EXPECT_OK(r.read(buf, &n, 1, sizeof(buf), o));
  EXPECT_EQ(sizeof(buf), n);
  EXPECT_EQ(expected, std::string(buf, n));

  n = 42;
  EXPECT_OK(r.read(buf, &n, sizeof(buf), sizeof(buf), o));
  EXPECT_EQ(sizeof(buf), n);
  EXPECT_EQ(expected, std::string(buf, n));
}

// }}}
// FDReader {{{

static void TestFDReader_Read(const base::Options& o) {
  base::Pipe pipe;
  ASSERT_OK(base::make_pipe(&pipe));

  LOG(INFO) << "made pipes";

  std::mutex mu;
  std::condition_variable cv;
  std::size_t x = 0, y = 0;

  std::thread t1([&pipe, &mu, &cv, &x, &y] {
    auto lock = base::acquire_lock(mu);

    while (x < 1) cv.wait(lock);
    LOG(INFO) << "T1 awoken: x = " << x;
    EXPECT_OK(base::write_exactly(pipe.write, "abcd", 4, "pipe"));
    LOG(INFO) << "wrote: abcd";

    while (x < 2) cv.wait(lock);
    LOG(INFO) << "T1 awoken: x = " << x;
    EXPECT_OK(base::write_exactly(pipe.write, "efgh", 4, "pipe"));
    LOG(INFO) << "wrote: efgh";

    ++y;
    cv.notify_all();
    LOG(INFO) << "woke T0: y = " << y;

    while (x < 3) cv.wait(lock);
    LOG(INFO) << "T1 awoken: x = " << x;
    EXPECT_OK(base::write_exactly(pipe.write, "ijkl", 4, "pipe"));
    LOG(INFO) << "wrote: ijkl";
  });

  LOG(INFO) << "spawned thread";

  io::Reader r = io::fdreader(pipe.read);

  LOG(INFO) << "made fdreader";

  char buf[8];
  std::size_t n;

  EXPECT_OK(r.read(buf, &n, 0, 8, o));
  EXPECT_EQ(0U, n);
  LOG(INFO) << "read zero bytes";

  auto lock = base::acquire_lock(mu);
  ++x;
  cv.notify_all();
  LOG(INFO) << "woke T1: x = " << x;
  lock.unlock();

  LOG(INFO) << "initiating read #1";
  EXPECT_OK(r.read(buf, &n, 1, 8, o));
  LOG(INFO) << "read #1 complete";
  EXPECT_EQ(4U, n);
  EXPECT_EQ("abcd", std::string(buf, n));

  lock.lock();
  ++x;
  cv.notify_all();
  LOG(INFO) << "woke T1: x = " << x;
  while (y < 1) cv.wait(lock);
  LOG(INFO) << "T0 awoken: y = " << y;
  lock.unlock();

  event::Task task;
  LOG(INFO) << "initiating read #2";
  r.read(&task, buf, &n, 8, 8, o);

  lock.lock();
  ++x;
  cv.notify_all();
  LOG(INFO) << "woke T1: x = " << x;
  lock.unlock();

  event::wait(io::get_manager(o), &task);
  LOG(INFO) << "read #2 complete";
  EXPECT_OK(task.result());
  EXPECT_EQ(8U, n);
  EXPECT_EQ("efghijkl", std::string(buf, n));

  t1.join();
  base::log_flush();
}

static void TestFDReader_WriteTo(const base::Options& o) {
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

  LOG(INFO) << "thread launched";

  io::Reader r = io::fdreader(fd);
  io::Writer w = io::fdwriter(s.left);

  event::Task task;
  std::size_t n;
  io::copy(&task, &n, w, r, o);

  auto lock = base::acquire_lock(mu);
  ready = true;
  cv.notify_all();
  lock.unlock();

  event::wait(io::get_manager(o), &task);
  LOG(INFO) << "task done";
  EXPECT_OK(task.result());
  EXPECT_EQ(16U * 4096U, n);

  ASSERT_OK(base::shutdown(s.left, SHUT_WR));
  t.join();
  LOG(INFO) << "thread done";
  EXPECT_EQ(sunk, n);

  base::log_flush();
}

TEST(FDReader, AsyncRead) {
  event::ManagerOptions mo;
  mo.set_async_mode();

  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options o;
  o.get<io::Options>().manager = m;

  TestFDReader_Read(o);

  m.shutdown();
}

TEST(FDReader, ThreadedRead) {
  event::ManagerOptions mo;
  mo.set_minimal_threaded_mode();

  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options o;
  o.get<io::Options>().manager = m;

  TestFDReader_Read(o);

  m.shutdown();
}

TEST(FDReader, WriteToFallback) {
  event::ManagerOptions mo;
  mo.set_async_mode();

  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options o;
  o.get<io::Options>().manager = m;
  o.get<io::Options>().transfer_mode = io::TransferMode::read_write;

  TestFDReader_WriteTo(o);

  m.shutdown();
}

TEST(FDReader, WriteToSendfile) {
  event::ManagerOptions mo;
  mo.set_async_mode();

  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options o;
  o.get<io::Options>().manager = m;
  o.get<io::Options>().transfer_mode = io::TransferMode::sendfile;

  TestFDReader_WriteTo(o);

  m.shutdown();
}

TEST(FDReader, WriteToSplice) {
  event::ManagerOptions mo;
  mo.set_async_mode();

  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options o;
  o.get<io::Options>().manager = m;
  o.get<io::Options>().transfer_mode = io::TransferMode::splice;

  TestFDReader_WriteTo(o);

  m.shutdown();
}

// }}}
// MultiReader {{{

void TestMultiReader_LineUp(const base::Options& o) {
  std::string a = "01234567";
  std::string b = "abcdefgh";
  std::string c = "ABCDEFGH";
  std::string d = "!@#$%^&*";

  std::string expected;
  expected.reserve(8 * 4);
  expected.append(a);
  expected.append(b);
  expected.append(c);
  expected.append(d);

  io::Reader r = io::multireader({
      io::stringreader(a), io::stringreader(b), io::stringreader(c),
      io::stringreader(d),
  });

  base::Result result;
  char buf[8];
  std::size_t n;
  std::string actual;

  while (true) {
    result = r.read(buf, &n, 8, 8, o);
    actual.append(buf, n);
    if (!result) break;
  }
  EXPECT_EOF(result);
  EXPECT_EQ(expected, actual);
}

void TestMultiReader_Half(const base::Options& o) {
  std::string a = "01234567";
  std::string b = "abcdefgh";

  std::string expected;
  expected.reserve(8 * 2);
  expected.append(a);
  expected.append(b);

  io::Reader r = io::multireader({
      io::stringreader(a), io::stringreader(b),
  });

  base::Result result;
  char buf[4];
  std::size_t n;
  std::string actual;

  while (true) {
    result = r.read(buf, &n, 4, 4, o);
    actual.append(buf, n);
    if (!result) break;
  }
  EXPECT_EOF(result);
  EXPECT_EQ(expected, actual);
}

void TestMultiReader_Double(const base::Options& o) {
  std::string a = "01234567";
  std::string b = "abcdefgh";
  std::string c = "ABCDEFGH";
  std::string d = "!@#$%^&*";

  std::string expected;
  expected.reserve(8 * 4);
  expected.append(a);
  expected.append(b);
  expected.append(c);
  expected.append(d);

  io::Reader r = io::multireader({
      io::stringreader(a), io::stringreader(b), io::stringreader(c),
      io::stringreader(d),
  });

  base::Result result;
  char buf[16];
  std::size_t n;
  std::string actual;

  while (true) {
    result = r.read(buf, &n, 16, 16, o);
    actual.append(buf, n);
    if (!result) break;
  }
  EXPECT_EOF(result);
  EXPECT_EQ(expected, actual);
}

void TestMultiReader_OffAxis(const base::Options& o) {
  std::string a = "01234";
  std::string b = "abcde";
  std::string c = "ABCDE";
  std::string d = "!@#$%";

  std::string expected;
  expected.reserve(5 * 4);
  expected.append(a);
  expected.append(b);
  expected.append(c);
  expected.append(d);

  io::Reader r = io::multireader({
      io::stringreader(a), io::stringreader(b), io::stringreader(c),
      io::stringreader(d),
  });

  base::Result result;
  char buf[8];
  std::size_t n;
  std::string actual;

  while (true) {
    result = r.read(buf, &n, 8, 8, o);
    actual.append(buf, n);
    if (!result) break;
  }
  EXPECT_EOF(result);
  EXPECT_EQ(expected, actual);
}

void TestMultiReader(const base::Options& o, const char* what) {
  LOG(INFO) << "[TestMultiReader_LineUp:" << what << "]";
  TestMultiReader_LineUp(o);
  LOG(INFO) << "[TestMultiReader_Half:" << what << "]";
  TestMultiReader_Half(o);
  LOG(INFO) << "[TestMultiReader_Double:" << what << "]";
  TestMultiReader_Double(o);
  LOG(INFO) << "[TestMultiReader_OffAxis:" << what << "]";
  TestMultiReader_OffAxis(o);
  LOG(INFO) << "[Done:" << what << "]";
  base::log_flush();
}

TEST(MultiReader, Async) {
  event::ManagerOptions mo;
  mo.set_async_mode();
  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options o;
  o.get<io::Options>().manager = m;
  TestMultiReader(o, "async");
  m.shutdown();
}

TEST(MultiReader, Threaded) {
  event::ManagerOptions mo;
  mo.set_threaded_mode();
  mo.set_num_pollers(2);
  mo.dispatcher().set_num_workers(2);
  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options o;
  o.get<io::Options>().manager = m;
  TestMultiReader(o, "threaded");
  m.shutdown();
}

// }}}
// BufferedReader {{{

static void TestBufferedReader(const base::Options& o, const char* what) {
  base::Pipe pipe;
  ASSERT_OK(base::make_pipe(&pipe));

  LOG(INFO) << "made pipes";

  std::mutex mu;
  std::condition_variable cv;
  std::size_t x = 0, y = 0;

  std::thread t1([&pipe, &mu, &cv, &x, &y] {
    auto lock = base::acquire_lock(mu);

    while (x < 1) cv.wait(lock);
    LOG(INFO) << "T1 awoken: x = " << x;
    EXPECT_OK(base::write_exactly(pipe.write, "abcd", 4, "pipe"));
    LOG(INFO) << "wrote: abcd";

    while (x < 2) cv.wait(lock);
    LOG(INFO) << "T1 awoken: x = " << x;
    EXPECT_OK(base::write_exactly(pipe.write, "efgh", 4, "pipe"));
    LOG(INFO) << "wrote: efgh";

    ++y;
    cv.notify_all();
    LOG(INFO) << "woke T0: y = " << y;

    while (x < 3) cv.wait(lock);
    LOG(INFO) << "T1 awoken: x = " << x;
    EXPECT_OK(base::write_exactly(pipe.write, "ijkl", 4, "pipe"));
    LOG(INFO) << "wrote: ijkl";
  });

  LOG(INFO) << "spawned thread";

  io::Reader r = io::bufferedreader(io::fdreader(pipe.read), 4U, 4U);

  LOG(INFO) << "made bufferedreader";

  char buf[8];
  std::size_t n;

  EXPECT_OK(r.read(buf, &n, 0, 8, o));
  EXPECT_EQ(0U, n);
  LOG(INFO) << "read zero bytes";

  auto lock = base::acquire_lock(mu);
  ++x;
  cv.notify_all();
  LOG(INFO) << "woke T1: x = " << x;
  lock.unlock();

  LOG(INFO) << "initiating read #1";
  EXPECT_OK(r.read(buf, &n, 1, 8, o));
  LOG(INFO) << "read #1 complete";
  EXPECT_EQ(4U, n);
  EXPECT_EQ("abcd", std::string(buf, n));

  lock.lock();
  ++x;
  cv.notify_all();
  LOG(INFO) << "woke T1: x = " << x;
  while (y < 1) cv.wait(lock);
  LOG(INFO) << "T0 awoken: y = " << y;
  lock.unlock();

  event::Task task;
  LOG(INFO) << "initiating read #2";
  r.read(&task, buf, &n, 8, 8, o);

  lock.lock();
  ++x;
  cv.notify_all();
  LOG(INFO) << "woke T1: x = " << x;
  lock.unlock();

  event::wait(io::get_manager(o), &task);
  LOG(INFO) << "read #2 complete";
  EXPECT_OK(task.result());
  EXPECT_EQ(8U, n);
  EXPECT_EQ("efghijkl", std::string(buf, n));

  t1.join();
  base::log_flush();
}

TEST(BufferedReader, Threaded) {
  event::ManagerOptions mo;
  mo.set_threaded_mode();
  mo.set_num_pollers(2);
  mo.dispatcher().set_num_workers(2);
  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options o;
  o.get<io::Options>().manager = m;
  TestBufferedReader(o, "threaded");
  m.shutdown();
}

// }}}

static void init() __attribute__((constructor));
static void init() {
  base::log_single_threaded();
  base::log_stderr_set_level(VLOG_LEVEL(6));
}
