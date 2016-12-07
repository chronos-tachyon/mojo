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
  event::wait_all({w.manager(), r.manager()}, {&task});
  EXPECT_NOT_IMPLEMENTED(task.result());
  EXPECT_EQ(0U, copied);
}

TEST(StringWriter, Close) {
  event::Task task;
  std::string out;
  io::Writer w = io::stringwriter(&out);
  w.close(&task);
  EXPECT_OK(task.result());
  task.reset();
  w.close(&task);
  EXPECT_FAILED_PRECONDITION(task.result());
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
  w.close(&task);
  EXPECT_OK(task.result());
  task.reset();
  w.close(&task);
  EXPECT_FAILED_PRECONDITION(task.result());
}

// }}}
// IgnoreCloseWriter {{{

TEST(IgnoreCloseWriter, Close) {
  int n = 0;
  auto wfn = [](event::Task* task, std::size_t* copied, const char* ptr,
                std::size_t len) {
    *copied = 0;
    if (task->start()) task->finish(base::Result::not_implemented());
  };
  auto cfn = [&n](event::Task* task) {
    ++n;
    if (task->start()) task->finish_ok();
  };

  io::Writer w;
  event::Task task;

  w = io::writer(wfn, cfn);

  w.close(&task);
  EXPECT_OK(task.result());
  EXPECT_EQ(1, n);

  task.reset();
  w.close(&task);
  EXPECT_OK(task.result());
  EXPECT_EQ(2, n);

  w = io::ignore_close(w);

  task.reset();
  w.close(&task);
  EXPECT_OK(task.result());
  EXPECT_EQ(2, n);
}

// }}}
// DiscardWriter {{{

TEST(DiscardWriter, Write) {
  std::size_t total = 42;
  io::Writer w = io::discardwriter(&total);
  EXPECT_EQ(0U, total);

  event::Task task;
  std::size_t n = 42;

  w.write(&task, &n, "abcdefgh", 8);
  event::wait(w.manager(), &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(8U, n);
  EXPECT_EQ(8U, total);

  task.reset();
  w.write(&task, &n, "ijkl", 4);
  event::wait(w.manager(), &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, n);
  EXPECT_EQ(12U, total);

  w = io::discardwriter();
  total = 0;

  task.reset();
  w.write(&task, &n, "abcdefgh", 8);
  event::wait(w.manager(), &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(8U, n);
  EXPECT_EQ(0U, total);

  task.reset();
  w.write(&task, &n, "ijkl", 4);
  event::wait(w.manager(), &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(4U, n);
  EXPECT_EQ(0U, total);
}

// }}}
// FullWriter {{{

TEST(FullWriter, Write) {
  io::Writer w = io::fullwriter(io::default_options());

  event::Task task;
  std::size_t n = 42;

  w.write(&task, &n, "", 0);
  event::wait(w.manager(), &task);
  EXPECT_OK(task.result());
  EXPECT_EQ(0U, n);

  task.reset();
  w.write(&task, &n, "a", 1);
  event::wait(w.manager(), &task);
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
  EXPECT_OK(base::set_blocking(pipe.read, true));

  LOG(INFO) << "made pipes";

  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));
  ASSERT_TRUE(m);

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
      if (n < 0) {
        int err_no = errno;
        if (err_no == EINTR) continue;
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

  io::Options o;
  o.set_manager(m);
  w = io::fdwriter(pipe.write, o);

  LOG(INFO) << "created fdwriter";

  buf.assign(1024, ++ch);
  w.write(&task, &n, buf.data(), buf.size());

  LOG(INFO) << "started write";

  std::unique_lock<std::mutex> lock(mu);
  ready = true;
  cv.notify_all();
  lock.unlock();

  event::wait(m, &task);
  expected.append(buf.data(), n);
  EXPECT_OK(task.result());
  EXPECT_EQ(1024U, n);

  LOG(INFO) << "wrote additional data";

  EXPECT_OK(pipe.write->close());

  LOG(INFO) << "closed pipe";

  lock.lock();
  while (!done) cv.wait(lock);
  EXPECT_EQ(expected, out);
}

TEST(FDWriter, InlineWrite) {
  event::ManagerOptions mo;
  mo.set_num_pollers(0, 1);
  mo.dispatcher().set_type(event::DispatcherType::inline_dispatcher);
  FDWriterTest(std::move(mo));
}

TEST(FDWriter, AsyncWrite) {
  event::ManagerOptions mo;
  mo.set_num_pollers(0, 1);
  mo.dispatcher().set_type(event::DispatcherType::async_dispatcher);
  FDWriterTest(std::move(mo));
}

TEST(FDWriter, ThreadedWrite) {
  event::ManagerOptions mo;
  mo.set_num_pollers(1);
  mo.dispatcher().set_type(event::DispatcherType::threaded_dispatcher);
  mo.dispatcher().set_num_workers(1);
  FDWriterTest(std::move(mo));
}

// }}}

static void init() __attribute__((constructor));
static void init() { base::log_stderr_set_level(VLOG_LEVEL(0)); }
