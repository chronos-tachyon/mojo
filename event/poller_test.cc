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
#include "base/mutex.h"
#include "base/result_testing.h"
#include "base/token.h"
#include "event/poller.h"

static void write_some_data(base::FD fd, uint32_t* counter) {
  static constexpr std::size_t len = sizeof(std::size_t);
  std::size_t value = ++*counter;
  auto pair = fd->acquire_fd();
  int n;
redo:
  n = ::write(pair.first, &value, sizeof(value));
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    throw std::runtime_error("failed write");
  }
  if (std::size_t(n) != len) throw std::runtime_error("partial write");
}

static void read_some_data(base::FD fd, uint32_t* counter) {
  static constexpr std::size_t len = sizeof(std::size_t);
  std::size_t value;
  auto pair = fd->acquire_fd();
  int n;
redo:
  value = 0;
  n = ::read(pair.first, &value, len);
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    throw std::runtime_error("failed read");
  }
  if (std::size_t(n) != len) throw std::runtime_error("partial read");
  ++*counter;
  EXPECT_EQ(value, *counter);
}

static void TestPollerImplementation(event::PollerPtr p) {
  base::SocketPair s;
  base::Result r = base::make_socketpair(&s, AF_UNIX, SOCK_STREAM, 0);
  ASSERT_OK(r);

  {
    auto pair = s.left->acquire_fd();
    EXPECT_EQ(0, ::shutdown(pair.first, SHUT_RD));
  }
  {
    auto pair = s.right->acquire_fd();
    EXPECT_EQ(0, ::shutdown(pair.first, SHUT_WR));
  }

  auto t = base::next_token();
  ASSERT_OK(p->add(s.right, t, event::Set::readable_bit()));

  event::Poller::EventVec vec;
  EXPECT_OK(p->wait(&vec, 0));
  EXPECT_EQ(0U, vec.size());

  uint32_t x = 0, y = 0;

  write_some_data(s.left, &x);
  vec.clear();
  EXPECT_TRUE(p->wait(&vec, 0));
  EXPECT_EQ(1U, vec.size());
  if (!vec.empty()) {
    EXPECT_EQ(t, vec.front().first);
    EXPECT_EQ(event::Set::readable_bit(), vec.front().second);
  }

  read_some_data(s.right, &y);
  EXPECT_TRUE(p->wait(&vec, 0));
  EXPECT_EQ(1U, vec.size());
  vec.clear();
  EXPECT_TRUE(p->wait(&vec, 0));
  EXPECT_EQ(0U, vec.size());

  std::mutex mu;
  std::condition_variable cv;
  bool ready = false;
  bool done = false;

  std::thread t1([&mu, &cv, &ready, &s, &x] {
    auto lock = base::acquire_lock(mu);
    while (!ready) cv.wait(lock);
    write_some_data(s.left, &x);
  });
  std::thread t2([&mu, &cv, &done, &p, &s, &y] {
    event::Poller::EventVec vec;
    EXPECT_TRUE(p->wait(&vec, -1));
    EXPECT_EQ(1U, vec.size());
    auto lock = base::acquire_lock(mu);
    read_some_data(s.right, &y);
    done = true;
    cv.notify_all();
  });

  auto lock = base::acquire_lock(mu);
  ready = true;
  cv.notify_all();
  while (!done) cv.wait(lock);
  lock.unlock();

  t1.join();
  t2.join();
}

TEST(Poller, Default) {
  event::PollerOptions o;
  event::PollerPtr p;
  ASSERT_OK(event::new_poller(&p, o));
  TestPollerImplementation(std::move(p));
}

TEST(Poller, EPoll) {
  event::PollerOptions o;
  o.set_type(event::PollerType::epoll_poller);
  event::PollerPtr p;
  ASSERT_OK(event::new_poller(&p, o));
  TestPollerImplementation(std::move(p));
}
