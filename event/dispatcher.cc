// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/dispatcher.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <tuple>
#include <typeinfo>

#include "base/cleanup.h"
#include "base/logging.h"
#include "base/util.h"

namespace {

struct Work {
  event::Task* task;
  event::CallbackPtr callback;

  Work(event::Task* task, event::CallbackPtr callback) noexcept
      : task(task),
        callback(std::move(callback)) {}
  Work() noexcept : Work(nullptr, nullptr) {}
};

static void invoke(base::Lock& lock, std::size_t& busy, std::size_t& done,
                   std::size_t& caught, Work item) {
  bool threw = true;
  ++busy;
  auto cleanup0 = base::cleanup([&busy, &done, &caught, &threw] {
    --busy;
    ++done;
    if (threw) ++caught;
  });

  lock.unlock();
  auto cleanup1 = base::cleanup([&lock] { lock.lock(); });

  if (item.task == nullptr || item.task->start()) {
    try {
      base::Result result = item.callback->run();
      if (item.task != nullptr)
        item.task->finish(std::move(result));
      else
        result.expect_ok(__FILE__, __LINE__);
      threw = false;
    } catch (...) {
      std::exception_ptr eptr = std::current_exception();
      if (item.task != nullptr)
        item.task->finish_exception(eptr);
      else
        LOG_EXCEPTION(eptr);
    }
  }
  item.callback.reset();
}

static void invoke(base::Lock& lock, event::IdleFunction idle) {
  if (!idle) return;

  lock.unlock();
  auto cleanup = base::cleanup([&lock] { lock.lock(); });

  try {
    idle();
  } catch (...) {
    LOG_EXCEPTION(std::current_exception());
  }
}

static thread_local std::size_t l_depth = 0;

}  // anonymous namespace

namespace event {

base::Result Dispatcher::adjust(const DispatcherOptions& opts) {
  return base::Result::not_implemented();
}

base::Result Dispatcher::cork() { return base::Result::not_implemented(); }

base::Result Dispatcher::uncork() { return base::Result::not_implemented(); }

namespace {

// The implementation for inline Dispatchers is fairly minimal.
class InlineDispatcher : public Dispatcher {
 public:
  InlineDispatcher() noexcept : busy_(0), done_(0), caught_(0) {}

  DispatcherType type() const noexcept override {
    return DispatcherType::inline_dispatcher;
  }

  void dispatch(Task* task, CallbackPtr callback) override {
    auto lock = base::acquire_lock(mu_);
    invoke(lock, busy_, done_, caught_, Work(task, std::move(callback)));
  }

  DispatcherStats stats() const override {
    auto lock = base::acquire_lock(mu_);
    DispatcherStats tmp;
    tmp.active_count = busy_;
    tmp.completed_count = done_;
    tmp.caught_exceptions = caught_;
    return tmp;
  }

  void shutdown() override {
    // noop
  }

  base::Result donate(bool forever) override { return base::Result(); }

 private:
  mutable std::mutex mu_;
  std::size_t busy_;
  std::size_t done_;
  std::size_t caught_;
};

// The implementation for async Dispatchers is slightly more complex.
class AsyncDispatcher : public Dispatcher {
 public:
  AsyncDispatcher(IdleFunction idle) noexcept : idle_(std::move(idle)),
                                                busy_(0),
                                                done_(0),
                                                caught_(0) {}

  DispatcherType type() const noexcept override {
    return DispatcherType::async_dispatcher;
  }

  void dispatch(Task* task, CallbackPtr callback) override {
    auto lock = base::acquire_lock(mu_);
    work_.emplace(task, std::move(callback));
  }

  DispatcherStats stats() const override {
    auto lock = base::acquire_lock(mu_);
    DispatcherStats tmp;
    tmp.pending_count = work_.size();
    tmp.active_count = busy_;
    tmp.completed_count = done_;
    tmp.caught_exceptions = caught_;
    return tmp;
  }

  void shutdown() override {
    // noop
  }

  base::Result donate(bool forever) override {
    CHECK_EQ(l_depth, 0U);
    ++l_depth;
    auto cleanup = base::cleanup([] { --l_depth; });

    auto lock = base::acquire_lock(mu_);
    Work item;
    while (!work_.empty()) {
      item = std::move(work_.front());
      work_.pop();
      invoke(lock, busy_, done_, caught_, std::move(item));
    }
    invoke(lock, idle_);
    return base::Result();
  }

 private:
  mutable std::mutex mu_;
  std::queue<Work> work_;
  IdleFunction idle_;
  std::size_t busy_;
  std::size_t done_;
  std::size_t caught_;
};

// The threaded implementation of Dispatcher is much more complex than that of
// the other two.  The basic idea is to match threads to workload.
class ThreadPoolDispatcher : public Dispatcher {
 public:
  ThreadPoolDispatcher(IdleFunction idle, std::size_t min, std::size_t max)
      : idle_(std::move(idle)),
        min_(min),
        max_(max),
        desired_(min),
        current_(0),
        busy_(0),
        done_(0),
        caught_(0),
        corked_(false) {
    if (min > max) {
      LOG(DFATAL) << "BUG: min > max";
      max = min;
    }
    auto lock = base::acquire_lock(mu_);
    ensure();
    while (current_ < min_) curr_cv_.wait(lock);
  }

  ~ThreadPoolDispatcher() noexcept override { shutdown(); }

  DispatcherType type() const noexcept override {
    return DispatcherType::threaded_dispatcher;
  }

  void dispatch(Task* task, CallbackPtr callback) override {
    auto lock = base::acquire_lock(mu_);
    work_.emplace(task, std::move(callback));
    if (corked_) return;

    std::size_t n = work_.size();
    // HEURISTIC: if queue size is greater than num threads, add a thread.
    //            (Threads that haven't finished starting add to the count.)
    //            This is (intentionally) a fairly aggressive growth policy.
    if (desired_ < max_ && n >= desired_) {
      ++desired_;
      ensure();
    }
    work_cv_.notify_one();
  }

  DispatcherStats stats() const override {
    auto lock = base::acquire_lock(mu_);
    DispatcherStats tmp;
    tmp.min_workers = min_;
    tmp.max_workers = max_;
    tmp.desired_num_workers = desired_;
    tmp.current_num_workers = current_;
    tmp.pending_count = work_.size();
    tmp.active_count = busy_;
    tmp.completed_count = done_;
    tmp.caught_exceptions = caught_;
    tmp.corked = corked_;
    return tmp;
  }

  void shutdown() override {
    auto lock = base::acquire_lock(mu_);
    min_ = max_ = desired_ = 0;
    while (current_ > desired_) curr_cv_.wait(lock);
  }

  base::Result adjust(const DispatcherOptions& opts) override {
    auto lock = base::acquire_lock(mu_);
    std::size_t min, max;
    bool has_min, has_max;
    std::tie(has_min, min) = opts.min_workers();
    std::tie(has_max, max) = opts.max_workers();
    if (!has_min) min = min_;
    if (!has_max) max = std::max(min, max_);
    if (min > max)
      return base::Result::invalid_argument(
          "bad event::DispatcherOptions: min_workers > max_workers");
    min_ = min;
    max_ = max;
    if (desired_ < min_) desired_ = min_;
    if (desired_ > max_) desired_ = max_;
    ensure();
    // Block until the thread count is within the new bounds.
    while (current_ < min_) curr_cv_.wait(lock);
    while (current_ > max_) curr_cv_.wait(lock);
    return base::Result();
  }

  base::Result cork() override {
    auto lock = base::acquire_lock(mu_);
    if (corked_)
      return base::Result::failed_precondition(
          "event::Dispatcher is already corked");
    corked_ = true;
    while (busy_ > 0) busy_cv_.wait(lock);
    return base::Result();
  }

  base::Result uncork() override {
    auto lock = base::acquire_lock(mu_);
    if (!corked_)
      return base::Result::failed_precondition(
          "event::Dispatcher is not corked");
    corked_ = false;
    if (!work_.empty()) {
      std::size_t n = work_.size();
      // HEURISTIC: when uncorking, aggressively spawn 1 thread per callback.
      n = std::min(n, max_);
      if (n > desired_) {
        desired_ = n;
        ensure();
      }
      work_cv_.notify_all();
    }
    return base::Result();
  }

  base::Result donate(bool forever) override {
    CHECK_EQ(l_depth, 0U);
    ++l_depth;
    auto cleanup0 = base::cleanup([] { --l_depth; });

    using MS = std::chrono::milliseconds;
    static constexpr MS kInitialTimeout = MS(125);
    static constexpr MS kMaximumTimeout = MS(8000);

    Work item;
    MS ms(kInitialTimeout);
    auto lock = base::acquire_lock(mu_);
    ++current_;
    curr_cv_.notify_all();
    auto cleanup1 = base::cleanup([this] {
      --current_;
      curr_cv_.notify_all();
    });

    while (true) {
      while (!corked_ && !work_.empty()) {
        if (current_ > max_) break;
        ms = kInitialTimeout;
        item = std::move(work_.front());
        work_.pop();
        invoke(lock, busy_, done_, caught_, std::move(item));
        if (busy_ == 0) busy_cv_.notify_all();
      }
      if (current_ > desired_) break;
      invoke(lock, idle_);
      if (!corked_ && !work_.empty()) continue;
      if (!forever) break;
      if (work_cv_.wait_for(lock, ms) == std::cv_status::timeout) {
        // HEURISTIC: If we've waited too long (approx. 2*kMaximumTimeout) with
        //            no work coming from the queue, then reduce the num
        //            threads by one.
        //
        //            Each worker thread is doing this calculation in parallel,
        //            so if five threads all reach this threshold, then the num
        //            threads will be reduced by five. The net effect is that
        //            all idle threads above the minimum will be aggressively
        //            pruned once sufficient time has passed.
        if (ms < kMaximumTimeout) {
          ms *= 2;
        } else {
          if (desired_ > min_) --desired_;
        }
      }
    }
    return base::Result();
  }

 private:
  void ensure() {
    auto closure = [this] { donate(true).ignore_ok(); };
    for (std::size_t i = current_, j = desired_; i < j; ++i) {
      std::thread t(closure);
      t.detach();
    }
  }

  mutable std::mutex mu_;
  std::condition_variable work_cv_;
  std::condition_variable curr_cv_;
  std::condition_variable busy_cv_;
  std::queue<Work> work_;
  IdleFunction idle_;
  std::size_t min_;
  std::size_t max_;
  std::size_t desired_;
  std::size_t current_;
  std::size_t busy_;
  std::size_t done_;
  std::size_t caught_;
  bool corked_;
};

}  // anonymous namespace

base::Result new_dispatcher(DispatcherPtr* out, const DispatcherOptions& opts) {
  auto idle = opts.idle_function();
  auto type = opts.type();
  std::size_t min, max;
  bool has_min, has_max;
  std::tie(has_min, min) = opts.min_workers();
  std::tie(has_max, max) = opts.max_workers();

  switch (type) {
    case DispatcherType::inline_dispatcher:
      *out = std::make_shared<InlineDispatcher>();
      break;

    case DispatcherType::unspecified:
    case DispatcherType::async_dispatcher:
      *out = std::make_shared<AsyncDispatcher>(std::move(idle));
      break;

    case DispatcherType::threaded_dispatcher:
      if (!has_min) min = 1;
      if (!has_max) max = std::max(min, num_cores());
      if (min > max)
        return base::Result::invalid_argument(
            "bad event::DispatcherOptions: min_workers > max_workers");
      *out = std::make_shared<ThreadPoolDispatcher>(std::move(idle), min, max);
      break;

    case DispatcherType::system_dispatcher:
      *out = system_dispatcher();
      break;

    default:
      return base::Result::not_implemented();
  }
  return base::Result();
}

static std::mutex g_sys_mu;
static DispatcherPtr* g_sys_i = nullptr;
static DispatcherPtr* g_sys_d = nullptr;

DispatcherPtr system_inline_dispatcher() {
  auto lock = base::acquire_lock(g_sys_mu);
  if (g_sys_i == nullptr) g_sys_i = new DispatcherPtr;
  if (!*g_sys_i) *g_sys_i = std::make_shared<InlineDispatcher>();
  return *g_sys_i;
}

DispatcherPtr system_dispatcher() {
  auto lock = base::acquire_lock(g_sys_mu);
  if (g_sys_d == nullptr) g_sys_d = new DispatcherPtr;
  if (!*g_sys_d)
    *g_sys_d = std::make_shared<ThreadPoolDispatcher>(nullptr, 1, num_cores());
  return *g_sys_d;
}

void set_system_dispatcher(DispatcherPtr ptr) {
  auto lock = base::acquire_lock(g_sys_mu);
  if (g_sys_d == nullptr) g_sys_d = new DispatcherPtr;
  g_sys_d->swap(ptr);
}

}  // namespace event
