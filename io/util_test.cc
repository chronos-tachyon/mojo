// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <array>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "base/cleanup.h"
#include "base/fd.h"
#include "base/logging.h"
#include "base/result_testing.h"
#include "io/util.h"

static std::unique_lock<std::mutex> acquire_lock(std::mutex& mu) {
  return std::unique_lock<std::mutex>(mu);
}

static void sleep_ms(unsigned int ms) {
  struct timespec tv;
  ::bzero(&tv, sizeof(tv));
  tv.tv_sec = (ms / 1000);
  tv.tv_nsec = (ms % 1000) * 1000000;
  ::nanosleep(&tv, nullptr);
}

static event::Manager make_manager() {
  event::Manager m;
  event::ManagerOptions mo;
  mo.set_num_pollers(0, 1);
  mo.dispatcher().set_type(event::DispatcherType::async_dispatcher);
  event::new_manager(&m, mo).assert_ok();
  return m;
}

TEST(Copy, StringReaderStringWriter) {
  std::string in = "0123456789ab";
  std::string out;
  io::Reader r = io::stringreader(in);
  io::Writer w = io::stringwriter(&out);

  event::Task task;
  std::size_t n;
  io::copy(&task, &n, w, r);
  event::wait_all({w.manager(), r.manager()}, {&task});
  EXPECT_OK(task.result());
  EXPECT_EQ(12U, n);
  EXPECT_EQ(out, in);
}

static void TestFileFileCopy(io::Options o) {
  std::string srcpath, dstpath;
  base::FD srcfd, dstfd;

  ASSERT_OK(base::make_tempfile(&srcpath, &srcfd, "mojo-io-util-test.XXXXXX"));
  auto cleanup0 = base::cleanup([srcpath] { ::unlink(srcpath.c_str()); });

  ASSERT_OK(base::make_tempfile(&dstpath, &dstfd, "mojo-io-util-test.XXXXXX"));
  auto cleanup1 = base::cleanup([dstpath] { ::unlink(dstpath.c_str()); });

  constexpr std::size_t N = 4096;

  std::vector<char> in;
  in.assign(N, 'A');
  in.resize(N, 'B');

  io::Reader r;
  io::Writer w;
  event::Task task;
  std::size_t n = 42;

  w = io::fdwriter(srcfd, o);
  w.write(&task, &n, in.data(), N);
  while (!task.is_finished()) {
    EXPECT_OK(o.manager().donate(false));
  }
  EXPECT_OK(task.result());
  EXPECT_EQ(N, n);

  {
    auto pair = srcfd->acquire_fd();
    EXPECT_EQ(0, ::lseek(pair.first, 0, SEEK_SET));
  }

  r = io::fdreader(srcfd, o);
  w = io::fdwriter(dstfd, o);
  task.reset();
  io::copy_n(&task, &n, N, w, r);
  while (!task.is_finished()) {
    EXPECT_OK(o.manager().donate(false));
  }
  EXPECT_OK(task.result());
  EXPECT_EQ(N, n);

  {
    auto pair = dstfd->acquire_fd();
    EXPECT_EQ(0, ::lseek(pair.first, 0, SEEK_SET));
  }

  std::vector<char> out;
  out.resize(2 * N);
  r = io::fdreader(dstfd, o);
  task.reset();
  r.read(&task, out.data(), &n, 1, out.size());
  while (!task.is_finished()) {
    EXPECT_OK(o.manager().donate(false));
  }
  EXPECT_OK(task.result());
  EXPECT_EQ(N, n);
  out.resize(n);

  EXPECT_EQ(std::string(in.begin(), in.end()),
            std::string(out.begin(), out.end()));
}

TEST(Copy, FileFileLoop512) {
  io::Options o;
  o.set_manager(make_manager());
  o.set_block_size(512);
  o.set_transfer_mode(io::TransferMode::read_write);
  TestFileFileCopy(o);
}

TEST(Copy, FileFileLoop4K) {
  io::Options o;
  o.set_manager(make_manager());
  o.set_block_size(4096);
  o.set_transfer_mode(io::TransferMode::read_write);
  TestFileFileCopy(o);
}

TEST(Copy, FileFileSendfile) {
  io::Options o;
  o.set_manager(make_manager());
  o.set_transfer_mode(io::TransferMode::sendfile);
  TestFileFileCopy(o);
}

TEST(Copy, FileFileSplice) {
  io::Options o;
  o.set_manager(make_manager());
  o.set_transfer_mode(io::TransferMode::splice);
  TestFileFileCopy(o);
}

TEST(Copy, HeterogenousManagers) {
  event::Manager rm = make_manager();
  event::Manager wm = make_manager();
  io::Options ro, wo;
  ro.set_manager(rm);
  ro.set_block_size(4096);
  ro.set_transfer_mode(io::TransferMode::read_write);
  wo.set_manager(wm);
  wo.set_block_size(4096);
  wo.set_transfer_mode(io::TransferMode::read_write);

  base::SocketPair rdpair, wrpair;
  ASSERT_OK(base::make_socketpair(&rdpair, AF_UNIX, SOCK_STREAM, 0));
  ASSERT_OK(base::make_socketpair(&wrpair, AF_UNIX, SOCK_STREAM, 0));
  ASSERT_OK(base::set_blocking(rdpair.left, true));
  ASSERT_OK(base::set_blocking(wrpair.right, true));
  ASSERT_OK(base::shutdown(rdpair.right, SHUT_WR));
  ASSERT_OK(base::shutdown(wrpair.right, SHUT_WR));

  LOG(INFO) << "sockets are ready";

  std::mutex mu;
  std::size_t blocks_transferred = 0;

  std::thread t1([&rdpair] {
    std::vector<char> buf;
    buf.assign(4096, 'A');
    for (std::size_t i = 0; i < 16; ++i) {
      sleep_ms(1);
      LOG(INFO) << "writing block #" << i;
      auto r =
          base::write_exactly(rdpair.left, buf.data(), buf.size(), "rdpair");
      EXPECT_OK(r);
      if (!r) break;
    }
    sleep_ms(1);
    LOG(INFO) << "sending EOF on rdpair";
    EXPECT_OK(rdpair.left->close());
  });

  std::thread t2([&wrpair, &mu, &blocks_transferred] {
    std::vector<char> buf;
    buf.resize(1024);
    std::size_t i = 0;
    base::Result r;
    while (true) {
      sleep_ms(1);
      LOG(INFO) << "reading block #" << (i / 4) << "." << ((i % 4) * 25);
      r = base::read_exactly(wrpair.right, buf.data(), buf.size(), "wrpair");
      if (!r) break;
      auto lock = acquire_lock(mu);
      ++blocks_transferred;
      ++i;
    }
    EXPECT_EOF(r);
    LOG(INFO) << "got EOF on wrpair";
  });

  io::Reader r = io::fdreader(rdpair.right, ro);
  io::Writer w = io::fdwriter(wrpair.left, wo);

  event::Task task;
  std::size_t n;
  LOG(INFO) << "starting copy";
  io::copy(&task, &n, w, r);
  LOG(INFO) << "waiting on copy";
  event::wait_all({rm, wm}, {&task});
  LOG(INFO) << "copy complete";
  EXPECT_OK(task.result());
  LOG(INFO) << "sending EOF on wrpair";
  EXPECT_OK(wrpair.left->close());

  LOG(INFO) << "joining threads";
  t1.join();
  t2.join();
  EXPECT_EQ(4U * 16U, blocks_transferred);
}

static void init() __attribute__((constructor));
static void init() { base::log_stderr_set_level(VLOG_LEVEL(6)); }
