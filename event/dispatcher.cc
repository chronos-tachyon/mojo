// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/dispatcher.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <tuple>
#include <typeinfo>

#include "base/cleanup.h"
#include "base/logging.h"
#include "base/util.h"

static thread_local std::size_t l_depth = 0;

namespace event {

namespace {

struct Work {
  Task* task;
  CallbackPtr callback;

  Work(Task* task, CallbackPtr callback) noexcept
      : task(task),
        callback(std::move(callback)) {}
  Work() noexcept : Work(nullptr, nullptr) {}
};

static void invoke(base::Lock& lock, std::size_t& busy, std::size_t& done,
                   std::size_t& caught, Work item) noexcept {
  bool threw = true;
  ++busy;
  auto cleanup = base::cleanup([&busy, &done, &caught, &threw] {
    --busy;
    ++done;
    if (threw) ++caught;
  });

  lock.unlock();
  auto reacquire = base::cleanup([&lock] { lock.lock(); });

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

static void invoke(base::Lock& lock, IdleFunction idle) noexcept {
  if (!idle) return;

  lock.unlock();
  auto reacquire = base::cleanup([&lock] { lock.lock(); });

  try {
    idle();
  } catch (...) {
    LOG_EXCEPTION(std::current_exception());
  }
}

}  // anonymous namespace

namespace internal {
void assert_depth() { CHECK_EQ(l_depth, 0U); }
}  // namespace internal

base::Result Dispatcher::adjust(const DispatcherOptions& opts) noexcept {
  return base::Result::not_implemented();
}

void Dispatcher::shutdown() noexcept {}
void Dispatcher::cork() noexcept {}
void Dispatcher::uncork() noexcept {}
void Dispatcher::donate(bool forever) noexcept {}

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

  DispatcherStats stats() const noexcept override {
    auto lock = base::acquire_lock(mu_);
    DispatcherStats tmp;
    tmp.active_count = busy_;
    tmp.completed_count = done_;
    tmp.caught_exceptions = caught_;
    return tmp;
  }

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
    work_.emplace_back(task, std::move(callback));
  }

  DispatcherStats stats() const noexcept override {
    auto lock = base::acquire_lock(mu_);
    DispatcherStats tmp;
    tmp.pending_count = work_.size();
    tmp.active_count = busy_;
    tmp.completed_count = done_;
    tmp.caught_exceptions = caught_;
    return tmp;
  }

  void donate(bool forever) noexcept override {
    internal::assert_depth();
    auto lock = base::acquire_lock(mu_);
    Work item;
    while (!work_.empty()) {
      item = std::move(work_.front());
      work_.pop_front();
      ++l_depth;
      auto cleanup = base::cleanup([] { --l_depth; });
      invoke(lock, busy_, done_, caught_, std::move(item));
    }
    invoke(lock, idle_);
  }

 private:
  mutable std::mutex mu_;
  std::deque<Work> work_;
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
    auto lock1 = base::acquire_lock(mu1_);
    ensure(lock1);
  }

  ~ThreadPoolDispatcher() noexcept override { shutdown(); }

  DispatcherType type() const noexcept override {
    return DispatcherType::threaded_dispatcher;
  }

  void dispatch(Task* task, CallbackPtr callback) override {
    auto lock0 = base::acquire_lock(mu0_);
    std::size_t n = work_.size();
    work_.emplace_back(task, std::move(callback));
    if (corked_) return;
    work_cv_.notify_one();
    lock0.unlock();

    // HEURISTIC: if queue size is greater than num threads, add a thread.
    //            (Threads that haven't finished starting add to the count.)
    //            This is (intentionally) a fairly aggressive growth policy.
    auto lock1 = base::acquire_lock(mu1_);
    if (desired_ < max_ && n > desired_) {
      ++desired_;
      ensure(lock1);
    }
  }

  DispatcherStats stats() const noexcept override {
    auto lock0 = base::acquire_lock(mu0_);
    auto lock1 = base::acquire_lock(mu1_);
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

  void shutdown() noexcept override {
    auto lock1 = base::acquire_lock(mu1_);
    min_ = max_ = desired_ = 0;
    ensure(lock1);
  }

  base::Result adjust(const DispatcherOptions& opts) noexcept override {
    std::size_t min, max;
    bool has_min, has_max;
    std::tie(has_min, min) = opts.min_workers();
    std::tie(has_max, max) = opts.max_workers();

    auto lock1 = base::acquire_lock(mu1_);
    if (!has_min) min = min_;
    if (!has_max) max = std::max(min, max_);
    if (min > max)
      return base::Result::invalid_argument(
          "bad event::DispatcherOptions: min_workers > max_workers");

    min_ = min;
    max_ = max;
    if (desired_ < min_) desired_ = min_;
    if (desired_ > max_) desired_ = max_;
    ensure(lock1);
    return base::Result();
  }

  void cork() noexcept override {
    auto lock0 = base::acquire_lock(mu0_);
    CHECK(!corked_);
    corked_ = true;
    while (busy_ != 0) busy_cv_.wait(lock0);
  }

  void uncork() noexcept override {
    auto lock0 = base::acquire_lock(mu0_);
    CHECK(corked_);
    corked_ = false;
    std::size_t n = work_.size();
    if (n > 1)
      work_cv_.notify_all();
    else if (n == 1)
      work_cv_.notify_one();
    lock0.unlock();

    auto lock1 = base::acquire_lock(mu1_);
    n = std::min(n, max_);
    // HEURISTIC: when uncorking, aggressively spawn 1 thread per callback.
    if (n > desired_) {
      desired_ = n;
      ensure(lock1);
    }
  }

  void donate(bool forever) noexcept override {
    internal::assert_depth();
    auto lock0 = base::acquire_lock(mu0_);
    if (forever)
      donate_forever(lock0);
    else
      donate_once(lock0);
  }

 private:
  struct Monitor {
    ThreadPoolDispatcher* d;
    bool is_live;

    Monitor(const Monitor&) = delete;
    Monitor(Monitor&&) = delete;
    Monitor& operator=(const Monitor&) = delete;
    Monitor& operator=(Monitor&&) = delete;

    explicit Monitor(ThreadPoolDispatcher* d) noexcept : d(d), is_live(false) {
      auto lock1 = base::acquire_lock(d->mu1_);
      inc(lock1);
    }

    ~Monitor() noexcept {
      auto lock1 = base::acquire_lock(d->mu1_);
      if (is_live) dec(lock1);
    }

    bool maybe_exit() noexcept {
      auto lock1 = base::acquire_lock(d->mu1_);
      if (d->current_ > d->desired_) {
        dec(lock1);
        return true;
      }
      return false;
    }

    bool too_many() noexcept {
      auto lock1 = base::acquire_lock(d->mu1_);
      if (d->desired_ > d->min_) {
        --d->desired_;
        dec(lock1);
        return true;
      }
      return false;
    }

    void inc(base::Lock& lock1) noexcept {
      CHECK(!is_live);
      is_live = true;
      ++d->current_;
      if (d->current_ == d->desired_) d->curr_cv_.notify_all();
    }

    void dec(base::Lock& lock1) noexcept {
      CHECK(is_live);
      is_live = false;
      --d->current_;
      if (d->current_ == d->desired_) d->curr_cv_.notify_all();
    }
  };

  bool has_work() { return !corked_ && !work_.empty(); }

  void donate_once(base::Lock& lock0) noexcept {
    Work item;
    while (has_work()) {
      item = std::move(work_.front());
      work_.pop_front();
      ++l_depth;
      auto cleanup = base::cleanup([] { --l_depth; });
      invoke(lock0, busy_, done_, caught_, std::move(item));
    }
    if (busy_ == 0) busy_cv_.notify_all();
    invoke(lock0, idle_);
  }

  void donate_forever(base::Lock& lock0) noexcept {
    using MS = std::chrono::milliseconds;
    static constexpr MS kInitialTimeout = MS(125);
    static constexpr MS kMaximumTimeout = MS(8000);

    Monitor monitor(this);
    Work item;
    MS ms(kInitialTimeout);
    while (true) {
      while (has_work()) {
        if (monitor.maybe_exit()) return;
        ms = kInitialTimeout;
        item = std::move(work_.front());
        work_.pop_front();
        ++l_depth;
        auto cleanup = base::cleanup([] { --l_depth; });
        invoke(lock0, busy_, done_, caught_, std::move(item));
      }
      if (busy_ == 0) busy_cv_.notify_all();
      if (monitor.maybe_exit()) return;
      invoke(lock0, idle_);
      if (has_work()) continue;
      if (work_cv_.wait_for(lock0, ms) == std::cv_status::timeout) {
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
        } else if (monitor.too_many()) {
          return;
        }
      }
    }
  }

  void ensure(base::Lock& lock1) {
    CHECK_LE(min_, max_);
    CHECK_LE(min_, desired_);
    CHECK_LE(desired_, max_);

    if (current_ < desired_) {
      // Spin up new threads.
      std::size_t delta = desired_ - current_;
      while (delta != 0) {
        auto closure = [this] { donate(true); };
        std::thread(closure).detach();
        --delta;
      }
    } else if (current_ > desired_) {
      // Wake up existing threads to self-terminate.
      lock1.unlock();
      auto reacquire = base::cleanup([&lock1] { lock1.lock(); });
      auto lock0 = base::acquire_lock(mu0_);
      work_cv_.notify_all();
    }

    // Block until the thread count has stabilized.
    while (current_ != desired_) curr_cv_.wait(lock1);

    CHECK_LE(min_, max_);
    CHECK_LE(min_, desired_);
    CHECK_LE(desired_, max_);
    CHECK_EQ(desired_, current_);
  }

  mutable std::mutex mu0_;
  mutable std::mutex mu1_;
  std::condition_variable work_cv_;  // mu0_: !work_.empty()
  std::condition_variable busy_cv_;  // mu0_: busy_ == 0
  std::condition_variable curr_cv_;  // mu1_: current_ == desired_
  std::deque<Work> work_;            // protected by mu0_
  IdleFunction idle_;                // protected by mu0_
  std::size_t min_;                  // protected by mu1_
  std::size_t max_;                  // protected by mu1_
  std::size_t desired_;              // protected by mu1_
  std::size_t current_;              // protected by mu1_
  std::size_t busy_;                 // protected by mu0_
  std::size_t done_;                 // protected by mu0_
  std::size_t caught_;               // protected by mu0_
  bool corked_;                      // protected by mu0_
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
