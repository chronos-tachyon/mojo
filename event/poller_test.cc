// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "base/cleanup.h"
#include "base/result_testing.h"
#include "event/poller.h"

static std::unique_lock<std::mutex> acquire(std::mutex& mu) {
  return std::unique_lock<std::mutex>(mu);
}

static void write_some_data(int fd, uint32_t* counter) {
  static constexpr std::size_t len = sizeof(std::size_t);
  std::size_t value = ++*counter;
  int n;
redo:
  n = ::write(fd, &value, sizeof(value));
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    throw std::runtime_error("failed write");
  }
  if (std::size_t(n) != len) throw std::runtime_error("partial write");
}

static void read_some_data(int fd, uint32_t* counter) {
  static constexpr std::size_t len = sizeof(std::size_t);
  std::size_t value;
  int n;
redo:
  value = 0;
  n = ::read(fd, &value, len);
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    throw std::runtime_error("failed read");
  }
  if (std::size_t(n) != len) throw std::runtime_error("partial read");
  ++*counter;
  EXPECT_EQ(value, *counter);
}

static void TestPollerImplementation(std::unique_ptr<event::Poller> p) {
  int fds[2] = {-1, -1};
  ASSERT_EQ(0, ::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                            0, fds));
  const int fd0 = fds[0];
  const int fd1 = fds[1];
  auto cleanup0 = base::cleanup([fd0] { ::close(fd0); });
  auto cleanup1 = base::cleanup([fd1] { ::close(fd1); });

  EXPECT_EQ(0, ::shutdown(fd0, SHUT_RD));
  EXPECT_EQ(0, ::shutdown(fd1, SHUT_WR));

  ASSERT_OK(p->add(fd1, event::Set::readable_bit()));

  std::vector<std::pair<int, event::Set>> vec;
  EXPECT_OK(p->wait(&vec, 0));
  EXPECT_EQ(0U, vec.size());

  uint32_t x = 0, y = 0;

  write_some_data(fd0, &x);
  vec.clear();
  EXPECT_TRUE(p->wait(&vec, 0));
  EXPECT_EQ(1U, vec.size());
  if (!vec.empty()) {
    EXPECT_EQ(fd1, vec.front().first);
    EXPECT_EQ(event::Set::readable_bit(), vec.front().second);
  }

  read_some_data(fd1, &y);
  EXPECT_TRUE(p->wait(&vec, 0));
  EXPECT_EQ(1U, vec.size());
  vec.clear();
  EXPECT_TRUE(p->wait(&vec, 0));
  EXPECT_EQ(0U, vec.size());

  std::mutex mu;
  std::condition_variable cv;
  bool ready = false;
  bool done = false;

  std::thread t1([&mu, &cv, &ready, fd0, &x] {
    auto lock = acquire(mu);
    while (!ready) cv.wait(lock);
    write_some_data(fd0, &x);
  });
  std::thread t2([&mu, &cv, &done, &p, fd1, &y] {
    std::vector<std::pair<int, event::Set>> vec;
    EXPECT_TRUE(p->wait(&vec, -1));
    EXPECT_EQ(1U, vec.size());
    auto lock = acquire(mu);
    read_some_data(fd1, &y);
    done = true;
    cv.notify_all();
  });

  auto lock = acquire(mu);
  ready = true;
  cv.notify_all();
  while (!done) cv.wait(lock);
  lock.unlock();

  t1.join();
  t2.join();
}

TEST(Poller, Default) {
  event::PollerOptions o;
  std::unique_ptr<event::Poller> p;
  ASSERT_OK(event::new_poller(&p, o));
  TestPollerImplementation(std::move(p));
}

TEST(Poller, EPoll) {
  event::PollerOptions o;
  o.set_type(event::PollerType::epoll_poller);
  std::unique_ptr<event::Poller> p;
  ASSERT_OK(event::new_poller(&p, o));
  TestPollerImplementation(std::move(p));
}
