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
#include "base/fd.h"
#include "base/logging.h"
#include "base/result_testing.h"
#include "base/util.h"
#include "event/manager.h"
#include "event/task.h"

static constexpr char kHelloWorld[] = "Hello, world!\n";
static constexpr std::size_t kHelloLen = sizeof(kHelloWorld) - 1;

static void write_some_data(base::FD fd, const void* ptr, std::size_t len) {
  auto pair = fd->acquire_fd();
  int n;
redo:
  n = ::write(pair.first, ptr, len);
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    throw std::runtime_error("failed write");
  }
  if (std::size_t(n) != len) throw std::runtime_error("partial write");
}

static void read_some_data(base::FD fd, const void* ptr, std::size_t len) {
  auto pair = fd->acquire_fd();
  char buf[16];
  int n;
redo:
  ::bzero(buf, sizeof(buf));
  n = ::read(pair.first, buf, sizeof(buf));
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    throw std::runtime_error("failed read");
  }
  if (std::size_t(n) != len) throw std::runtime_error("partial read");
  if (memcmp(buf, ptr, len) != 0) throw std::runtime_error("data mismatch");
}

using IntPredicate = std::function<bool(int)>;

static void TestManagerImplementation_FDs(event::Manager m) {
  base::SocketPair s;
  base::Result r = base::make_socketpair(&s, AF_UNIX, SOCK_STREAM, 0);
  EXPECT_OK(r);

  {
    auto pair = s.left->acquire_fd();
    EXPECT_EQ(0, ::shutdown(pair.first, SHUT_RD));
  }
  {
    auto pair = s.right->acquire_fd();
    EXPECT_EQ(0, ::shutdown(pair.first, SHUT_WR));
  }

  event::Task task;
  auto closure = [&s, &task](event::Data data) {
    LOG(INFO) << "hello from closure";
    read_some_data(s.right, kHelloWorld, kHelloLen);
    task.finish_ok();
    LOG(INFO) << "task: finished";
    return base::Result();
  };

  EXPECT_TRUE(task.start());

  LOG(INFO) << "registering fd";
  event::FileDescriptor fd;
  EXPECT_OK(
      m.fd(&fd, s.right, event::Set::readable_bit(), event::handler(closure)));

  LOG(INFO) << "writing data";
  write_some_data(s.left, kHelloWorld, kHelloLen);

  LOG(INFO) << "task: waiting for finish";
  event::wait(m, &task);
  LOG(INFO) << "task: got finish";
  EXPECT_OK(task.result());

  LOG(INFO) << "before release";
  EXPECT_OK(fd.release());
  LOG(INFO) << "after release";
  EXPECT_OK(s.left->close());
  EXPECT_OK(s.right->close());
  LOG(INFO) << "after close";
}

static void TestManagerImplementation_Signals(event::Manager m) {
  event::Task task;
  std::mutex mu;
  int flags = 0;
  IntPredicate predicate = [](int flags) { return flags == 7; };

  auto handler = [&task, &mu, &flags, &predicate](int bit, event::Data data) {
    LOG(INFO) << "hello from handler";
    auto lock = base::acquire_lock(mu);
    flags |= bit;
    LOG(INFO) << "flags = " << flags;
    bool done = predicate(flags);
    lock.unlock();
    if (done && !task.is_finished()) {
      task.finish_ok();
      LOG(INFO) << "task: finished";
    }
    return base::Result();
  };

  EXPECT_TRUE(task.start());

  LOG(INFO) << "registering signals";
  event::Signal hup, usr1, usr2;
  EXPECT_OK(m.signal(&hup, SIGHUP, event::handler(handler, 1)));
  EXPECT_OK(m.signal(&usr1, SIGUSR1, event::handler(handler, 2)));
  EXPECT_OK(m.signal(&usr2, SIGUSR2, event::handler(handler, 4)));

  LOG(INFO) << "sending signals";
  ::kill(::getpid(), SIGHUP);
  ::kill(::getpid(), SIGUSR1);
  ::kill(::getpid(), SIGUSR2);

  LOG(INFO) << "task: waiting for finish";
  event::wait(m, &task);
  LOG(INFO) << "task: got finish";
  auto lock = base::acquire_lock(mu);
  EXPECT_OK(task.result());
  EXPECT_EQ(7, flags);
  lock.unlock();

  LOG(INFO) << "before release";
  EXPECT_OK(usr2.release());
  EXPECT_OK(usr1.release());
  EXPECT_OK(hup.release());
  LOG(INFO) << "after release";
}

static void TestManagerImplementation_Timers(event::Manager m) {
  event::Task task;
  std::mutex mu;
  int counter = 0;
  IntPredicate predicate = [](int counter) { return counter >= 5; };

  auto timer_closure = [&task, &mu, &counter, &predicate](event::Data data) {
    LOG(INFO) << "hello from timer handler, int_value = " << data.int_value;
    auto lock = base::acquire_lock(mu);
    counter += data.int_value;
    bool done = predicate(counter);
    lock.unlock();
    if (done && !task.is_finished()) {
      task.finish_ok();
      LOG(INFO) << "task: finished";
    }
    return base::Result();
  };

  EXPECT_TRUE(task.start());
  auto lock = base::acquire_lock(mu);

  LOG(INFO) << "registering timer";
  event::Timer t;
  EXPECT_OK(m.timer(&t, event::handler(timer_closure)));

  LOG(INFO) << "setting timer to period 1ms";
  EXPECT_OK(t.set_periodic(base::milliseconds(1)));

  lock.unlock();
  LOG(INFO) << "task: waiting for finish";
  event::wait(m, &task);
  LOG(INFO) << "task: got finish";
  lock.lock();
  EXPECT_GE(counter, 5);
  EXPECT_OK(task.result());
  EXPECT_OK(t.cancel());

  task.reset();
  EXPECT_TRUE(task.start());
  counter = 0;
  predicate = [](int counter) { return counter != 0; };

  LOG(INFO) << "setting timer to oneshot now+5ms";
  EXPECT_OK(t.set_at(base::monotonic_now() + base::milliseconds(5)));

  lock.unlock();
  LOG(INFO) << "task: waiting for finish";
  event::wait(m, &task);
  LOG(INFO) << "got: counter = " << counter;
  lock.lock();
  EXPECT_OK(task.result());
  EXPECT_GE(counter, 1);

  LOG(INFO) << "before release";
  EXPECT_OK(t.release());
  LOG(INFO) << "after release";
}

static void TestManagerImplementation_Events(event::Manager m) {
  event::Task task;
  std::mutex mu;
  std::condition_variable cv;
  bool ready = false;

  auto handler = [&task](event::Data data) {
    LOG(INFO) << "hello from generic event handler, int_value = "
            << data.int_value;
    if (data.int_value == 42)
      task.finish_ok();
    else
      task.finish(base::Result::internal("BUG"));
    return base::Result();
  };

  EXPECT_TRUE(task.start());

  LOG(INFO) << "registering event";
  event::Generic e;
  EXPECT_OK(m.generic(&e, event::handler(handler)));

  LOG(INFO) << "spawning thread";
  std::thread thd([&mu, &cv, &ready, &e] {
    LOG(INFO) << "hello from thread";
    auto lock = base::acquire_lock(mu);
    while (!ready) cv.wait(lock);
    LOG(INFO) << "got: ready = true";
    e.fire(42);
    LOG(INFO) << "event has been fired";
  });

  LOG(INFO) << "notify: ready = true";
  auto lock = base::acquire_lock(mu);
  ready = true;
  cv.notify_all();
  lock.unlock();
  LOG(INFO) << "task: waiting for finish";
  event::wait(m, &task);
  LOG(INFO) << "task: got finish";
  EXPECT_OK(task.result());

  LOG(INFO) << "joining thread";
  thd.join();

  LOG(INFO) << "before release";
  EXPECT_OK(e.release());
  LOG(INFO) << "after release";
}

static void TestManagerImplementation_TaskTimeouts(event::Manager m) {
  std::mutex mu;
  unsigned int a = 1;
  unsigned int b = 1;
  event::Task task;
  event::Timer t;

  auto closure = [&mu, &a, &b, &task, &t](event::Data data) {
    if (task.is_running()) {
      auto lock = base::acquire_lock(mu);
      auto i = data.int_value;
      while (i > 0) {
        unsigned int c = a + b;
        LOG(INFO) << "a(" << a << ") + b(" << b << ") = c(" << c << ")";
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

  base::MonotonicTime at = base::monotonic_now() + base::milliseconds(3);
  EXPECT_OK(m.set_deadline(&task, at));

  EXPECT_TRUE(task.start());
  EXPECT_TRUE(task.is_running());
  EXPECT_OK(m.timer(&t, event::handler(closure)));
  EXPECT_OK(t.set_periodic(base::milliseconds(1)));
  event::wait(m, &task);
  EXPECT_DEADLINE_EXCEEDED(task.result());
}

static void TestManagerImplementation(event::Manager m, std::string name) {
  LOG(INFO) << "[" << name << ":TestManagerImplementation_FDs]";
  TestManagerImplementation_FDs(m);
  LOG(INFO) << "[" << name << ":TestManagerImplementation_Signals]";
  TestManagerImplementation_Signals(m);
  LOG(INFO) << "[" << name << ":TestManagerImplementation_Timers]";
  TestManagerImplementation_Timers(m);
  LOG(INFO) << "[" << name << ":TestManagerImplementation_Events]";
  TestManagerImplementation_Events(m);
  LOG(INFO) << "[" << name << ":TestManagerImplementation_TaskTimeouts]";
  TestManagerImplementation_TaskTimeouts(m);
  LOG(INFO) << "[" << name << ":shutdown]";
  EXPECT_OK(m.shutdown());
  LOG(INFO) << "OK";
}

TEST(Manager, DefaultDefault) {
  event::Manager m;
  event::ManagerOptions o;
  EXPECT_OK(event::new_manager(&m, o));
  TestManagerImplementation(m, "default/default");
}

TEST(Manager, DefaultAsync) {
  event::Manager m;
  event::ManagerOptions o;
  o.dispatcher().set_type(event::DispatcherType::async_dispatcher);
  EXPECT_OK(event::new_manager(&m, o));
  TestManagerImplementation(m, "default/async");
}

TEST(Manager, AsyncAsync) {
  event::Manager m;
  event::ManagerOptions o;
  o.set_num_pollers(0, 1);
  o.dispatcher().set_type(event::DispatcherType::async_dispatcher);
  EXPECT_OK(event::new_manager(&m, o));
  TestManagerImplementation(m, "async/async");
}

TEST(Manager, AsyncInline) {
  event::Manager m;
  event::ManagerOptions o;
  o.set_num_pollers(0, 1);
  o.dispatcher().set_type(event::DispatcherType::inline_dispatcher);
  EXPECT_OK(event::new_manager(&m, o));
  TestManagerImplementation(m, "async/inline");
}

static void init() __attribute__((constructor));
static void init() { base::log_stderr_set_level(VLOG_LEVEL(0)); }
