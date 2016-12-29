// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <array>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#include "base/logging.h"
#include "base/result_testing.h"
#include "event/dispatcher.h"

namespace event {
static bool operator==(const DispatcherStats& a, const DispatcherStats& b) {
  return a.min_workers == b.min_workers && a.max_workers == b.max_workers &&
         a.desired_num_workers == b.desired_num_workers &&
         a.current_num_workers == b.current_num_workers &&
         a.pending_count == b.pending_count &&
         a.active_count == b.active_count &&
         a.completed_count == b.completed_count &&
         a.caught_exceptions == b.caught_exceptions && a.corked == b.corked;
}
}  // namespace event

static bool equalish(const event::DispatcherStats& a,
                     const event::DispatcherStats& b) {
  return a.min_workers == b.min_workers && a.max_workers == b.max_workers &&
         a.pending_count == b.pending_count &&
         a.active_count == b.active_count &&
         a.completed_count == b.completed_count &&
         a.caught_exceptions == b.caught_exceptions && a.corked == b.corked &&
         (a.desired_num_workers >= a.min_workers &&
          a.desired_num_workers <= a.max_workers &&
          b.desired_num_workers >= b.min_workers &&
          b.desired_num_workers <= b.max_workers) &&
         (a.current_num_workers >= a.min_workers &&
          a.current_num_workers <= a.max_workers &&
          b.current_num_workers >= b.min_workers &&
          b.current_num_workers <= b.max_workers);
}

static std::ostream& operator<<(std::ostream& os,
                                const event::DispatcherStats& s) {
  os << "Stats("
     << "min=" << s.min_workers << ","
     << "max=" << s.max_workers << ","
     << "desired=" << s.desired_num_workers << ","
     << "current=" << s.current_num_workers << ","
     << "pending=" << s.pending_count << ","
     << "active=" << s.active_count << ","
     << "completed=" << s.completed_count << ","
     << "exceptions=" << s.caught_exceptions << ","
     << "corked=" << s.corked << ")";
  return os;
}

struct Closure {
  int* counter;

  Closure(int* counter) noexcept : counter(counter) {}
  base::Result operator()() {
    ++*counter;
    if (*counter >= 3)
      return base::Result::out_of_range("my spoon is too big");
    else
      return base::Result();
  }
};

struct Predicate {
  int* counter;

  Predicate(int* counter) noexcept : counter(counter) {}
  bool operator()() { return *counter < 3; }
};

struct Throw {
  base::Result operator()() {
    throw std::system_error(EDOM, std::system_category(), "foo");
  }
};

TEST(InlineDispatcher, EndToEnd) {
  event::DispatcherOptions o;
  o.set_type(event::DispatcherType::inline_dispatcher);
  event::DispatcherPtr d;
  ASSERT_OK(event::new_dispatcher(&d, o));

  int n = 0;

  event::Task task;
  d->dispatch(&task, event::callback(Closure(&n)));
  EXPECT_EQ(1, n);
  EXPECT_OK(task.result());

  task.reset();
  d->dispatch(&task, event::callback(Closure(&n)));
  EXPECT_EQ(2, n);
  EXPECT_OK(task.result());

  task.reset();
  d->dispatch(&task, event::callback(Closure(&n)));
  EXPECT_EQ(3, n);
  EXPECT_OUT_OF_RANGE(task.result());

  event::DispatcherStats expected;
  expected.completed_count = 3;
  EXPECT_EQ(expected, d->stats());
}

TEST(InlineDispatcher, ThrowingCallback) {
  event::DispatcherOptions o;
  o.set_type(event::DispatcherType::inline_dispatcher);
  event::DispatcherPtr d;
  ASSERT_OK(event::new_dispatcher(&d, o));

  std::array<event::Task, 5> tasks;
  for (auto& task : tasks) {
    d->dispatch(&task, event::callback(Throw()));
  }
  for (auto& task : tasks) {
    EXPECT_THROW(task.result(), std::system_error);
  }

  event::DispatcherStats expected;
  expected.completed_count = 5;
  expected.caught_exceptions = 5;
  EXPECT_EQ(expected, d->stats());
}

TEST(AsyncDispatcher, EndToEnd) {
  event::DispatcherOptions o;
  o.set_type(event::DispatcherType::async_dispatcher);
  event::DispatcherPtr d;
  ASSERT_OK(event::new_dispatcher(&d, o));

  int n = 0;

  event::Task task1, task2, task3;
  d->dispatch(&task1, event::callback(Closure(&n)));
  d->dispatch(&task2, event::callback(Closure(&n)));
  d->dispatch(&task3, event::callback(Closure(&n)));
  EXPECT_EQ(0, n);
  d->donate(false);
  EXPECT_EQ(3, n);
  EXPECT_OK(task1.result());
  EXPECT_OK(task2.result());
  EXPECT_OUT_OF_RANGE(task3.result());

  event::DispatcherStats expected;
  expected.completed_count = 3;
  EXPECT_EQ(expected, d->stats());
}

TEST(AsyncDispatcher, ThrowingCallback) {
  event::DispatcherOptions o;
  o.set_type(event::DispatcherType::async_dispatcher);
  event::DispatcherPtr d;
  ASSERT_OK(event::new_dispatcher(&d, o));

  std::array<event::Task, 5> tasks;
  for (auto& task : tasks) {
    d->dispatch(&task, event::callback(Throw()));
  }
  d->donate(false);
  for (auto& task : tasks) {
    EXPECT_THROW(task.result(), std::system_error);
  }

  event::DispatcherStats expected;
  expected.completed_count = 5;
  expected.caught_exceptions = 5;
  EXPECT_EQ(expected, d->stats());
}

TEST(ThreadPoolDispatcher, EndToEnd) {
  event::DispatcherOptions o;
  o.set_type(event::DispatcherType::threaded_dispatcher);
  o.set_num_workers(1, 4);
  event::DispatcherPtr d;
  ASSERT_OK(event::new_dispatcher(&d, o));

  event::DispatcherStats expected;
  expected.min_workers = 1;
  expected.max_workers = 4;
  expected.desired_num_workers = 1;
  expected.current_num_workers = 1;
  EXPECT_EQ(expected, d->stats());

  d->cork();

  expected.corked = true;
  EXPECT_EQ(expected, d->stats());

  std::mutex mu;
  std::condition_variable cv;
  int n = 0;
  bool done = false;

  auto inc_callback = [&mu, &cv, &n](std::size_t i) {
    EXPECT_LT(i, 10U);
    auto lock = base::acquire_lock(mu);
    LOG(INFO) << "hello from increment callback #" << i;
    ++n;
    cv.notify_all();
    return base::Result();
  };

  auto done_callback = [&mu, &cv, &n, &done] {
    auto lock = base::acquire_lock(mu);
    LOG(INFO) << "hello from done callback";
    while (n < 10) cv.wait(lock);
    done = true;
    cv.notify_all();
    LOG(INFO) << "done";
    return base::Result();
  };

  std::array<event::Task, 10> tasks;
  std::size_t i;
  for (i = 0; i < 10; ++i) {
    d->dispatch(&tasks[i], event::callback(inc_callback, i));
  }
  i = 42;
  event::Task donetask;
  d->dispatch(&donetask, event::callback(done_callback));

  auto lock = base::acquire_lock(mu);
  EXPECT_EQ(0, n);
  EXPECT_FALSE(done);
  lock.unlock();

  expected.pending_count = 11;
  EXPECT_EQ(expected, d->stats());

  d->uncork();

  lock.lock();
  LOG(INFO) << "waiting on done";
  while (!done) cv.wait(lock);
  LOG(INFO) << "got done";
  lock.unlock();

  d->cork();
  d->uncork();
  LOG(INFO) << "corked and uncorked";

  EXPECT_EQ(10, n);
  for (i = 0; i < 10; i++) {
    EXPECT_OK(tasks[i].result());
  }
  EXPECT_OK(donetask.result());

  expected.desired_num_workers = 4;
  expected.current_num_workers = 4;
  expected.pending_count = 0;
  expected.completed_count = 11;
  expected.corked = false;
  EXPECT_PRED2(equalish, expected, d->stats());

  o.set_num_workers(2, 3);
  EXPECT_OK(d->adjust(o));

  expected.min_workers = 2;
  expected.max_workers = 3;
  expected.desired_num_workers = 3;
  expected.current_num_workers = 3;
  EXPECT_PRED2(equalish, expected, d->stats());

  LOG(INFO) << "waiting on shutdown";
  d->shutdown();
  LOG(INFO) << "got shutdown";

  expected.min_workers = 0;
  expected.max_workers = 0;
  expected.desired_num_workers = 0;
  expected.current_num_workers = 0;
  EXPECT_EQ(expected, d->stats());
}

TEST(ThreadPoolDispatcher, ThrowingCallback) {
  event::DispatcherOptions o;
  o.set_type(event::DispatcherType::threaded_dispatcher);
  o.set_num_workers(1, 2);
  event::DispatcherPtr d;
  ASSERT_OK(event::new_dispatcher(&d, o));

  event::DispatcherStats expected;
  expected.min_workers = 1;
  expected.max_workers = 2;
  expected.desired_num_workers = 1;
  expected.current_num_workers = 1;
  EXPECT_EQ(expected, d->stats());

  LOG(INFO) << "dispatching callbacks that throw";
  std::array<event::Task, 5> tasks;
  for (std::size_t i = 0; i < 5; ++i) {
    d->dispatch(&tasks[i], event::callback(Throw()));
  }

  do {
    std::this_thread::yield();
  } while (d->stats().incomplete_count() != 0);

  for (std::size_t i = 0; i < 5; ++i) {
    EXPECT_THROW(tasks[i].result(), std::system_error);
  }

  expected.completed_count = 5;
  expected.caught_exceptions = 5;
  EXPECT_PRED2(equalish, expected, d->stats());
}

static void init() __attribute__((constructor));
static void init() { base::log_stderr_set_level(VLOG_LEVEL(0)); }
