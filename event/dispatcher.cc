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

struct restore_depth {
  void operator()() const noexcept { --l_depth; }
};

struct reacquire_lock {
  base::Lock& lock;

  explicit reacquire_lock(base::Lock& lk) noexcept : lock(lk) {}
  void operator()() const noexcept { lock.lock(); }
};

struct dispatch_thread {
  Dispatcher* dispatcher;

  explicit dispatch_thread(Dispatcher* d) noexcept
      : dispatcher(DCHECK_NOTNULL(d)) {}
  void operator()() const noexcept { dispatcher->donate(true); }
};

struct Work {
  Task* task;
  CallbackPtr callback;

  Work(Task* task, CallbackPtr callback) noexcept
      : task(task),
        callback(std::move(callback)) {}
  Work() noexcept : Work(nullptr, nullptr) {}
};

struct invoke_helper {
  std::size_t* const busy;
  std::size_t* const done;
  std::size_t* const caught;
  bool threw;

  invoke_helper(std::size_t* b, std::size_t* d, std::size_t* c) noexcept
      : busy(DCHECK_NOTNULL(b)),
        done(DCHECK_NOTNULL(d)),
        caught(DCHECK_NOTNULL(c)),
        threw(true) {
    ++*busy;
  }

  ~invoke_helper() noexcept {
    --*busy;
    ++*done;
    if (threw) ++*caught;
  }

  void safe() noexcept { threw = false; }
};

static void invoke(base::Lock& lock, std::size_t* busy, std::size_t* done,
                   std::size_t* caught, Work item) noexcept {
  invoke_helper helper(busy, done, caught);
  lock.unlock();
  auto reacquire = base::cleanup(reacquire_lock(lock));

  if (item.task == nullptr || item.task->start()) {
    try {
      base::Result result = item.callback->run();
      if (item.task != nullptr)
        item.task->finish(std::move(result));
      else
        result.expect_ok(__FILE__, __LINE__);
      helper.safe();
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

static void finalize(CallbackPtr finalizer) noexcept {
  try {
    DCHECK_NOTNULL(std::move(finalizer))->run().expect_ok(__FILE__, __LINE__);
  } catch (...) {
    LOG_EXCEPTION(std::current_exception());
  }
}

static void finalize(base::Lock& lock,
                     std::vector<CallbackPtr>& trash) noexcept {
  auto vec = std::move(trash);
  lock.unlock();
  auto reacquire = base::cleanup(reacquire_lock(lock));
  for (auto& finalizer : vec) {
    finalize(std::move(finalizer));
  }
}

// The implementation for inline Dispatchers is fairly minimal.
class InlineDispatcher : public Dispatcher {
 public:
  InlineDispatcher() noexcept : busy_(0), done_(0), caught_(0) {}

  DispatcherType type() const noexcept override {
    return DispatcherType::inline_dispatcher;
  }

  void dispatch(Task* task, CallbackPtr callback) override {
    auto lock = base::acquire_lock(mu_);
    invoke(lock, &busy_, &done_, &caught_, Work(task, std::move(callback)));
  }

  void dispose(CallbackPtr finalizer) override {
    finalize(std::move(finalizer));
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
  AsyncDispatcher() noexcept : busy_(0), done_(0), caught_(0) {}

  ~AsyncDispatcher() noexcept override {
    auto lock = base::acquire_lock(mu_);
    work_.clear();
    finalize(lock, trash_);
  }

  DispatcherType type() const noexcept override {
    return DispatcherType::async_dispatcher;
  }

  void dispatch(Task* task, CallbackPtr callback) override {
    auto lock = base::acquire_lock(mu_);
    work_.emplace_back(task, std::move(callback));
  }

  void dispose(CallbackPtr finalizer) override {
    auto lock = base::acquire_lock(mu_);
    trash_.push_back(std::move(finalizer));
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
      auto cleanup = base::cleanup(restore_depth());
      invoke(lock, &busy_, &done_, &caught_, std::move(item));
    }
    finalize(lock, trash_);
  }

 private:
  mutable std::mutex mu_;
  std::deque<Work> work_;
  std::vector<CallbackPtr> trash_;
  std::size_t busy_;
  std::size_t done_;
  std::size_t caught_;
};

struct thread_monitor {
  std::mutex* const mutex;
  std::condition_variable* const condvar;
  std::size_t* const min;
  std::size_t* const max;
  std::size_t* const desired;
  std::size_t* const current;
  bool is_live;

  explicit thread_monitor(std::mutex* mu, std::condition_variable* cv, std::size_t* mn,
                   std::size_t* mx, std::size_t* d, std::size_t* c) noexcept
      : mutex(DCHECK_NOTNULL(mu)),
        condvar(DCHECK_NOTNULL(cv)),
        min(DCHECK_NOTNULL(mn)),
        max(DCHECK_NOTNULL(mx)),
        desired(DCHECK_NOTNULL(d)),
        current(DCHECK_NOTNULL(c)),
        is_live(false) {
    auto lock = base::acquire_lock(*mutex);
    inc(lock);
  }

  thread_monitor(const thread_monitor&) = delete;
  thread_monitor(thread_monitor&&) = delete;
  thread_monitor& operator=(const thread_monitor&) = delete;
  thread_monitor& operator=(thread_monitor&&) = delete;

  ~thread_monitor() noexcept {
    auto lock = base::acquire_lock(*mutex);
    if (is_live) dec(lock);
  }

  bool maybe_exit() noexcept {
    auto lock = base::acquire_lock(*mutex);
    if (*current > *desired) {
      dec(lock);
      return true;
    }
    return false;
  }

  bool too_many() noexcept {
    auto lock = base::acquire_lock(*mutex);
    if (*desired > *min) {
      --*desired;
      dec(lock);
      return true;
    }
    return false;
  }

 private:
  void inc(base::Lock& lock) noexcept {
    CHECK(!is_live);
    is_live = true;
    ++*current;
    if (*current == *desired) condvar->notify_all();
  }

  void dec(base::Lock& lock) noexcept {
    CHECK(is_live);
    is_live = false;
    --*current;
    if (*current == *desired) condvar->notify_all();
  }
};

// The threaded implementation of Dispatcher is much more complex than that of
// the other two.  The basic idea is to match threads to workload.
class ThreadPoolDispatcher : public Dispatcher {
 public:
  ThreadPoolDispatcher(std::size_t min, std::size_t max)
      : min_(min),
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

  ~ThreadPoolDispatcher() noexcept override {
    shutdown();
    auto lock0 = base::acquire_lock(mu0_);
    work_.clear();
    finalize(lock0, trash_);
  }

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

  void dispose(CallbackPtr finalizer) override {
    auto lock0 = base::acquire_lock(mu0_);
    trash_.push_back(std::move(finalizer));
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

  void shutdown() noexcept override {
    auto lock1 = base::acquire_lock(mu1_);
    min_ = max_ = desired_ = 0;
    ensure(lock1);
  }

 private:
  bool has_work() const noexcept { return !corked_ && !work_.empty(); }

  void donate_once(base::Lock& lock0) noexcept {
    Work item;
    while (has_work()) {
      item = std::move(work_.front());
      work_.pop_front();
      ++l_depth;
      auto cleanup = base::cleanup(restore_depth());
      invoke(lock0, &busy_, &done_, &caught_, std::move(item));
    }
    if (busy_ == 0) busy_cv_.notify_all();
    finalize(lock0, trash_);
  }

  void donate_forever(base::Lock& lock0) noexcept {
    using MS = std::chrono::milliseconds;
    static constexpr MS kInitialTimeout = MS(125);
    static constexpr MS kMaximumTimeout = MS(8000);

    thread_monitor mon(&mu1_, &curr_cv_, &min_, &max_, &desired_, &current_);
    Work item;
    MS ms(kInitialTimeout);
    while (true) {
      while (has_work()) {
        if (mon.maybe_exit()) return;
        ms = kInitialTimeout;
        item = std::move(work_.front());
        work_.pop_front();
        ++l_depth;
        auto cleanup = base::cleanup(restore_depth());
        invoke(lock0, &busy_, &done_, &caught_, std::move(item));
      }
      if (busy_ == 0) busy_cv_.notify_all();
      if (mon.maybe_exit()) return;
      finalize(lock0, trash_);
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
        } else if (mon.too_many()) {
          return;
        }
      }
    }
  }

  void ensure(base::Lock& lock1) noexcept {
    CHECK_LE(min_, max_);
    CHECK_LE(min_, desired_);
    CHECK_LE(desired_, max_);

    if (current_ < desired_) {
      // Spin up new threads.
      std::size_t delta = desired_ - current_;
      while (delta != 0) {
        std::thread(dispatch_thread(this)).detach();
        --delta;
      }
    } else if (current_ > desired_) {
      // Wake up existing threads to self-terminate.
      lock1.unlock();
      auto reacquire = base::cleanup(reacquire_lock(lock1));
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
  std::vector<CallbackPtr> trash_;   // protected by mu0_
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

namespace internal {
void assert_depth() { CHECK_EQ(l_depth, 0U); }
}  // namespace internal

base::Result new_dispatcher(DispatcherPtr* out, const DispatcherOptions& opts) {
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
      *out = std::make_shared<AsyncDispatcher>();
      break;

    case DispatcherType::threaded_dispatcher:
      if (!has_min) min = 1;
      if (!has_max) max = std::max(min, num_cores());
      if (min > max)
        return base::Result::invalid_argument(
            "bad event::DispatcherOptions: min_workers > max_workers");
      *out = std::make_shared<ThreadPoolDispatcher>(min, max);
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
    *g_sys_d = std::make_shared<ThreadPoolDispatcher>(1, num_cores());
  return *g_sys_d;
}

void set_system_dispatcher(DispatcherPtr ptr) {
  auto lock = base::acquire_lock(g_sys_mu);
  if (g_sys_d == nullptr) g_sys_d = new DispatcherPtr;
  g_sys_d->swap(ptr);
}

}  // namespace event
