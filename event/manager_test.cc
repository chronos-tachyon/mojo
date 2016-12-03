// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "base/cleanup.h"
#include "base/clock.h"
#include "base/logging.h"
#include "base/result_testing.h"
#include "event/manager.h"
#include "event/task.h"

static constexpr char kHelloWorld[] = "Hello, world!\n";
static constexpr std::size_t kHelloLen = sizeof(kHelloWorld) - 1;

static std::unique_lock<std::mutex> acquire(std::mutex& mu) {
  return std::unique_lock<std::mutex>(mu);
}

static void write_some_data(int fd, const void* ptr, std::size_t len) {
  int n;
redo:
  n = ::write(fd, ptr, len);
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    throw std::runtime_error("failed write");
  }
  if (std::size_t(n) != len) throw std::runtime_error("partial write");
}

static void read_some_data(int fd, const void* ptr, std::size_t len) {
  char buf[16];
  int n;
redo:
  ::bzero(buf, sizeof(buf));
  n = ::read(fd, buf, sizeof(buf));
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    throw std::runtime_error("failed read");
  }
  if (std::size_t(n) != len) throw std::runtime_error("partial read");
  if (memcmp(buf, ptr, len) != 0) throw std::runtime_error("data mismatch");
}

using IntPredicate = std::function<bool(int)>;

static void TestManagerImplementation_FDs(event::Manager& m) {
  event::Task task;

  int fds[2] = {-1, -1};
  int rc =
      ::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds);
  EXPECT_EQ(0, rc);
  if (rc != 0) return;

  const int fd0 = fds[0];
  const int fd1 = fds[1];
  auto cleanup = base::cleanup([fd0, fd1] {
    ::close(fd0);
    ::close(fd1);
  });

  EXPECT_EQ(0, ::shutdown(fd0, SHUT_RD));
  EXPECT_EQ(0, ::shutdown(fd1, SHUT_WR));

  auto fd_closure = [&task, fd1](event::Data data) {
    VLOG(0) << "hello from fd_closure";
    read_some_data(fd1, kHelloWorld, kHelloLen);
    task.finish_ok();
    VLOG(0) << "task: finished";
    return base::Result();
  };

  ASSERT_TRUE(task.start());

  VLOG(0) << "registering fd";
  event::FileDescriptor fd;
  ASSERT_OK(
      m.fd(&fd, fd1, event::Set::readable_bit(), event::handler(fd_closure)));

  VLOG(0) << "writing data";
  write_some_data(fd0, kHelloWorld, kHelloLen);

  VLOG(0) << "task: waiting for finish";
  event::wait(m, &task);
  VLOG(0) << "task: got finish";
  EXPECT_OK(task.result());

  VLOG(0) << "before release";
  EXPECT_OK(fd.release());
  VLOG(0) << "after release";
}

static void TestManagerImplementation_Signals(event::Manager& m) {
  event::Task task;
  std::mutex mu;
  int flags = 0;
  IntPredicate predicate = [](int flags) { return flags == 7; };

  auto handler = [&task, &mu, &flags, &predicate](int bit, event::Data data) {
    VLOG(0) << "hello from handler";
    auto lock = acquire(mu);
    flags |= bit;
    VLOG(0) << "flags = " << flags;
    bool done = predicate(flags);
    lock.unlock();
    if (done && !task.is_finished()) {
      task.finish_ok();
      VLOG(0) << "task: finished";
    }
    return base::Result();
  };

  ASSERT_TRUE(task.start());

  VLOG(0) << "registering signals";
  event::Signal hup, usr1, usr2;
  ASSERT_OK(m.signal(&hup, SIGHUP, event::handler(handler, 1)));
  ASSERT_OK(m.signal(&usr1, SIGUSR1, event::handler(handler, 2)));
  ASSERT_OK(m.signal(&usr2, SIGUSR2, event::handler(handler, 4)));

  VLOG(0) << "sending signals";
  ::kill(::getpid(), SIGHUP);
  ::kill(::getpid(), SIGUSR1);
  ::kill(::getpid(), SIGUSR2);

  VLOG(0) << "task: waiting for finish";
  event::wait(m, &task);
  VLOG(0) << "task: got finish";
  auto lock = acquire(mu);
  EXPECT_OK(task.result());
  EXPECT_EQ(7, flags);
  lock.unlock();

  VLOG(0) << "before release";
  EXPECT_OK(usr2.release());
  EXPECT_OK(usr1.release());
  EXPECT_OK(hup.release());
  VLOG(0) << "after release";
}

static void TestManagerImplementation_Timers(event::Manager& m) {
  event::Task task;
  std::mutex mu;
  int counter = 0;
  IntPredicate predicate = [](int counter) { return counter >= 5; };

  auto timer_closure = [&task, &mu, &counter, &predicate](event::Data data) {
    VLOG(0) << "hello from timer handler, int_value = " << data.int_value;
    auto lock = acquire(mu);
    counter += data.int_value;
    bool done = predicate(counter);
    lock.unlock();
    if (done && !task.is_finished()) {
      task.finish_ok();
      VLOG(0) << "task: finished";
    }
    return base::Result();
  };

  ASSERT_TRUE(task.start());
  auto lock = acquire(mu);

  VLOG(0) << "registering timer";
  event::Timer t;
  ASSERT_OK(m.timer(&t, event::handler(timer_closure)));

  VLOG(0) << "setting timer to period 1ms";
  ASSERT_OK(t.set_periodic(base::milliseconds(1)));

  lock.unlock();
  VLOG(0) << "task: waiting for finish";
  event::wait(m, &task);
  VLOG(0) << "task: got finish";
  lock.lock();
  EXPECT_GE(counter, 5);
  EXPECT_OK(task.result());
  EXPECT_OK(t.cancel());

  task.reset();
  ASSERT_TRUE(task.start());
  counter = 0;
  predicate = [](int counter) { return counter != 0; };

  VLOG(0) << "setting timer to oneshot now+5ms";
  EXPECT_OK(t.set_at(base::monotonic_now() + base::milliseconds(5)));

  lock.unlock();
  VLOG(0) << "task: waiting for finish";
  event::wait(m, &task);
  VLOG(0) << "got: counter = " << counter;
  lock.lock();
  EXPECT_OK(task.result());
  EXPECT_GE(counter, 1);

  VLOG(0) << "before release";
  EXPECT_OK(t.release());
  VLOG(0) << "after release";
}

static void TestManagerImplementation_Events(event::Manager& m) {
  event::Task task;
  std::mutex mu;
  std::condition_variable cv;
  bool ready = false;

  auto handler = [&task](event::Data data) {
    VLOG(0) << "hello from generic event handler, int_value = "
            << data.int_value;
    if (data.int_value == 42)
      task.finish_ok();
    else
      task.finish(base::Result::internal("BUG"));
    return base::Result();
  };

  ASSERT_TRUE(task.start());

  VLOG(0) << "registering event";
  event::Generic e;
  ASSERT_OK(m.generic(&e, event::handler(handler)));

  VLOG(0) << "spawning thread";
  std::thread thd([&mu, &cv, &ready, &e] {
    VLOG(0) << "hello from thread";
    auto lock = acquire(mu);
    while (!ready) cv.wait(lock);
    VLOG(0) << "got: ready = true";
    e.fire(42);
    VLOG(0) << "event has been fired";
  });

  VLOG(0) << "notify: ready = true";
  auto lock = acquire(mu);
  ready = true;
  cv.notify_all();
  lock.unlock();
  VLOG(0) << "task: waiting for finish";
  event::wait(m, &task);
  VLOG(0) << "task: got finish";
  EXPECT_OK(task.result());

  VLOG(0) << "joining thread";
  thd.join();

  VLOG(0) << "before release";
  EXPECT_OK(e.release());
  VLOG(0) << "after release";
}

static void TestManagerImplementation_TaskTimeouts(event::Manager m) {
  std::mutex mu;
  unsigned int a = 1;
  unsigned int b = 1;
  event::Task task;
  event::Timer t;

  auto closure = [&mu, &a, &b, &task, &t] (event::Data data) {
    if (task.is_running()) {
      auto lock = acquire(mu);
      auto i = data.int_value;
      while (i > 0) {
        unsigned int c = a + b;
        VLOG(0) << "a(" << a << ") + b(" << b << ") = c(" << c << ")";
        a = b;
        b = c;
        --i;
      }
    } else {
      EXPECT_EQ(event::Task::State::expiring, task.state());
      EXPECT_OK(t.release());
      EXPECT_TRUE(task.finish_cancel());
    }
    return base::Result();
  };

  base::Time at = base::monotonic_now() + base::milliseconds(3);
  EXPECT_OK(m.set_deadline(&task, at));

  EXPECT_TRUE(task.start());
  EXPECT_TRUE(task.is_running());
  EXPECT_OK(m.timer(&t, event::handler(closure)));
  EXPECT_OK(t.set_periodic(base::milliseconds(1)));
  event::wait(m, &task);
  EXPECT_DEADLINE_EXCEEDED(task.result());
}

static void TestManagerImplementation(event::Manager m, std::string name) {
  VLOG(0) << "[" << name << ":TestManagerImplementation_FDs]";
  TestManagerImplementation_FDs(m);
  VLOG(0) << "[" << name << ":TestManagerImplementation_Signals]";
  TestManagerImplementation_Signals(m);
  VLOG(0) << "[" << name << ":TestManagerImplementation_Timers]";
  TestManagerImplementation_Timers(m);
  VLOG(0) << "[" << name << ":TestManagerImplementation_Events]";
  TestManagerImplementation_Events(m);
  VLOG(0) << "[" << name << ":TestManagerImplementation_TaskTimeouts]";
  TestManagerImplementation_TaskTimeouts(m);
  VLOG(0) << "[" << name << ":shutdown]";
  EXPECT_OK(m.shutdown());
  VLOG(0) << "OK";
}

TEST(Manager, DefaultDefault) {
  event::Manager m;
  event::ManagerOptions o;
  EXPECT_OK(event::new_manager(&m, o));
  TestManagerImplementation(std::move(m), "default/default");
}

TEST(Manager, DefaultAsync) {
  event::Manager m;
  event::ManagerOptions o;
  o.dispatcher().set_type(event::DispatcherType::async_dispatcher);
  EXPECT_OK(event::new_manager(&m, o));
  TestManagerImplementation(std::move(m), "default/async");
}

TEST(Manager, AsyncAsync) {
  event::Manager m;
  event::ManagerOptions o;
  o.set_num_pollers(0, 1);
  o.dispatcher().set_type(event::DispatcherType::async_dispatcher);
  EXPECT_OK(event::new_manager(&m, o));
  TestManagerImplementation(std::move(m), "async/async");
}

TEST(Manager, AsyncInline) {
  event::Manager m;
  event::ManagerOptions o;
  o.set_num_pollers(0, 1);
  o.dispatcher().set_type(event::DispatcherType::inline_dispatcher);
  EXPECT_OK(event::new_manager(&m, o));
  TestManagerImplementation(std::move(m), "async/inline");
}

static void init() __attribute__((constructor));
static void init() { base::log_stderr_set_level(0); }
