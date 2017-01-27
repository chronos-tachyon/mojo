// event/manager.h - Event-driven async or multithreaded I/O
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "base/clock.h"
#include "base/duration.h"
#include "base/fd.h"
#include "base/logging.h"
#include "base/result.h"
#include "base/time.h"
#include "base/token.h"
#include "event/callback.h"
#include "event/data.h"
#include "event/dispatcher.h"
#include "event/handler.h"
#include "event/poller.h"
#include "event/set.h"
#include "event/task.h"

namespace event {

namespace internal {

using CallbackVec = std::vector<CallbackPtr>;

struct Record {
  mutable std::mutex mu;
  std::condition_variable cv;  // outstanding == 0
  const base::token_t token;
  const DispatcherPtr dispatcher;
  const HandlerPtr handler;
  std::size_t outstanding;  // # of outstanding calls to |handler|
  Set set;
  bool disabled;  // true iff new calls forbidden

  Record(base::token_t t, DispatcherPtr d, HandlerPtr h, Set set) noexcept
      : token(t),
        dispatcher(std::move(d)),
        handler(std::move(h)),
        outstanding(0),
        set(set),
        disabled(false) {
    DCHECK_NOTNULL(dispatcher);
    DCHECK_NOTNULL(handler);
  }

  ~Record() noexcept { wait(); }

  void wait() noexcept;
};

enum class SourceType : uint8_t {
  undefined = 0,
  fd = 1,
  signal = 2,
  timer = 3,
  generic = 4,
};

struct Source {
  std::vector<Record*> records;
  base::FD fd;
  int signo;
  SourceType type;

  Source() noexcept : signo(0), type(SourceType::undefined) {}
};

class ManagerImpl;

struct HandlerCallback : public Callback {
  Record* rec;
  Data data;

  HandlerCallback(Record* r, Data d) noexcept : rec(DCHECK_NOTNULL(r)),
                                                data(std::move(d)) {
    ++rec->outstanding;
  }

  ~HandlerCallback() noexcept override;

  base::Result run() override;
};

class ManagerImpl {
 public:
  ManagerImpl(PollerPtr p, DispatcherPtr d, base::Pipe pipe, std::size_t num);

  ManagerImpl(const ManagerImpl&) = delete;
  ManagerImpl(ManagerImpl&&) = delete;
  ManagerImpl& operator=(const ManagerImpl&) = delete;
  ManagerImpl& operator=(ManagerImpl&&) = delete;

  ~ManagerImpl() noexcept { shutdown(); }

  bool is_running() const noexcept {
    auto lock = base::acquire_lock(mu_);
    return running_;
  }

  PollerPtr poller() const noexcept;
  DispatcherPtr dispatcher() const noexcept;

  base::Result fd_add(std::unique_ptr<Record>* out, base::FD fd, Set set,
                      HandlerPtr handler);
  base::Result signal_add(std::unique_ptr<Record>* out, int signo,
                          HandlerPtr handler);
  base::Result timer_add(std::unique_ptr<Record>* out, HandlerPtr handler);
  base::Result generic_add(std::unique_ptr<Record>* out, HandlerPtr handler);
  base::Result modify(Record* myrec, Set set);
  base::Result arm(Record* myrec, base::Duration delay,
                   base::Duration period, bool delay_abs);
  base::Result fire(Record* myrec, int value);
  base::Result disable(Record* myrec);

  void donate(bool forever) noexcept;
  void shutdown() noexcept;

 private:
  friend struct Record;
  friend struct HandlerCallback;

  void inc_current(base::Lock& lock) noexcept;
  void dec_current(base::Lock& lock) noexcept;

  void donate_once(base::Lock& lock) noexcept;
  void donate_forever(base::Lock& lock) noexcept;

  void schedule(CallbackVec* cbvec, Record* rec, Set set, Data data);
  void handle_event(CallbackVec* cbvec, base::token_t t, Set set);
  void handle_pipe_event(CallbackVec* cbvec);
  void handle_fd_event(CallbackVec* cbvec, base::token_t t, const Source& src,
                       Set set);
  void handle_timer_event(CallbackVec* cbvec, base::token_t t,
                          const Source& src);

  mutable std::mutex mu_;
  std::condition_variable curr_cv_;  // all changes to current_
  std::unordered_map<int, base::token_t> fdmap_;
  std::unordered_map<int, base::token_t> sigmap_;
  std::unordered_map<base::token_t, Source> sources_;
  PollerPtr p_;
  DispatcherPtr d_;
  base::Pipe pipe_;
  std::size_t num_;      // target # of poller threads
  std::size_t current_;  // current # of poller threads
  bool running_;         // true iff not shut down
};

}  // namespace internal

using ManagerPtr = std::shared_ptr<internal::ManagerImpl>;
using RecordPtr = std::unique_ptr<internal::Record>;

// An event::Handle binds an event handler to one or more events.
class Handle {
 public:
  // Handle is default constructible in the empty state.
  Handle() noexcept = default;

  // Handle is moveable but not copyable.
  Handle(const Handle&) = delete;
  Handle(Handle&&) noexcept = default;
  Handle& operator=(const Handle&) = delete;
  Handle& operator=(Handle&&) noexcept = default;

  // Swaps this Handle with another.
  void swap(Handle& x) noexcept {
    ptr_.swap(x.ptr_);
    rec_.swap(x.rec_);
  }

  // Returns true iff this Handle is non-empty.
  explicit operator bool() const noexcept { return !!rec_; }

  // Asserts that this Handle is non-empty.
  void assert_valid() const;

  // Gets the set of events which the Handler is interested in.
  base::Result get(Set* out) const;

  // Replaces the set of FD events which the Handler is interested in.
  base::Result modify(Set set) const;

  // Arms the timer event as a one-shot timer for the given absolute time.
  // The time is specified in terms of the |base::system_monotonic_clock()|.
  base::Result set_at(base::MonotonicTime at) const;
  base::Result set_at(base::Time at) const {
    return set_at(base::system_monotonic_clock().convert(at));
  }

  // Arms the timer event as a one-shot timer for a time relative to now.
  base::Result set_delay(base::Duration delay) const;

  // Arms the timer event as a periodic timer with the given period.
  base::Result set_periodic(base::Duration period) const;

  // Arms the timer event as a periodic timer with the given period.
  // The first event will arrive at the given absolute time.
  base::Result set_periodic_at(base::Duration period, base::MonotonicTime at) const;
  base::Result set_periodic_at(base::Duration period, base::Time at) const {
    return set_periodic_at(period, base::system_monotonic_clock().convert(at));
  }

  // Arms the timer event as a periodic timer with the given period.
  // The first event will arrive after the specified delay.
  base::Result set_periodic_delay(base::Duration period, base::Duration delay) const;

  // Disarms the timer event. The Handle remains valid, but it produces no
  // further events until it is armed again.
  base::Result cancel() const;

  // Triggers a generic event.
  // |value| will be available as |data.int_value| in the Handler.
  base::Result fire(int value = 0) const;

  // Unbinds the Handler from future file descriptor events.
  base::Result disable() const;

  // Waits for all Handler calls to complete.
  // PRECONDITION: |disable()| was called
  void wait();

  // Disowns any incomplete Handler calls.
  // PRECONDITION: |disable()| was called
  void disown();

  // Combines the effects of the |disable()| and |wait()| methods.
  base::Result release();

 private:
  friend class Manager;

  Handle(ManagerPtr ptr, RecordPtr rec) noexcept : ptr_(std::move(ptr)),
                                                   rec_(std::move(rec)) {}

  ManagerPtr ptr_;
  RecordPtr rec_;
};

inline void swap(Handle& a, Handle& b) noexcept { a.swap(b); }

// A ManagerOptions holds user-available choices in the configuration of
// Manager instances.
class ManagerOptions {
 private:
  enum bits {
    bit_num = (1U << 0),
  };

 public:
  // ManagerOptions is default constructible, copyable, and moveable.
  // There is intentionally no constructor for aggregate initialization.
  ManagerOptions() noexcept : num_(0), has_(0) {}
  ManagerOptions(const ManagerOptions&) = default;
  ManagerOptions(ManagerOptions&&) = default;
  ManagerOptions& operator=(const ManagerOptions&) = default;
  ManagerOptions& operator=(ManagerOptions&&) = default;

  // Resets all fields to their default values.
  void reset() { *this = ManagerOptions(); }

  // Options for configuring the Poller instance.
  PollerOptions& poller() noexcept { return poller_; }
  const PollerOptions& poller() const noexcept { return poller_; }

  // Options for configuring the Dispatcher instance.
  DispatcherOptions& dispatcher() noexcept { return dispatcher_; }
  const DispatcherOptions& dispatcher() const noexcept { return dispatcher_; }

  // |num_pollers()| is the number of threads that should dedicate their full
  // attention to event polling.
  // - |num_pollers().first| is true iff a custom value has been specified.
  // - |num_pollers().second| is the custom value, if present.
  std::pair<bool, std::size_t> num_pollers() const {
    return std::make_pair((has_ & bit_num) != 0, num_);
  }
  void reset_num_pollers() noexcept {
    has_ &= ~bit_num;
    num_ = 0;
  }
  void set_num_pollers(std::size_t num) noexcept {
    num_ = num;
    has_ |= bit_num;
  }

  // Convenience method for a single-threaded Manager with inline dispatching.
  void set_inline_mode() noexcept {
    set_num_pollers(0);
    dispatcher().set_type(DispatcherType::inline_dispatcher);
  }

  // Convenience method for a single-threaded Manager with async dispatching.
  void set_async_mode() noexcept {
    set_num_pollers(0);
    dispatcher().set_type(DispatcherType::async_dispatcher);
  }

  // Convenience method for a minimal multi-threaded Manager.
  void set_minimal_threaded_mode() noexcept {
    set_num_pollers(1);
    dispatcher().set_type(DispatcherType::threaded_dispatcher);
    dispatcher().set_num_workers(1);
  }

  // Convenience method for a standard multi-threaded Manager.
  void set_threaded_mode() noexcept {
    reset_num_pollers();
    dispatcher().set_type(DispatcherType::threaded_dispatcher);
    dispatcher().reset_num_workers();
  }

 private:
  PollerOptions poller_;
  DispatcherOptions dispatcher_;
  std::size_t num_;
  uint8_t has_;
};

// A Manager is a framework for various models of threaded and asynchronous
// event-based processing.
//
// THREAD SAFETY: This class is thread-safe.
//
// A simple example using threads:
//
//    event::Manager m;
//    event::ManagerOptions o;
//    CHECK_OK(new_manager(&m, o));
//
//    std::mutex mu;
//    std::condition_variable cv;
//    bool done = false;
//
//    event::Handle timer;
//    result = m.timer(&timer, event::handler([&] (event::Data data) {
//      auto lock = base::acquire_lock(mu);
//      std::cout << "Hello from timer!" << std::endl;
//      done = true;
//      cv.notify_all();
//      return base::Result();
//    }));
//    CHECK_OK(result);
//
//    result = timer.set_delay(base::seconds(10));
//    CHECK_OK(result);
//
//    std::unique_lock<std::mutex> lock(mu);
//    while (!done) cv.wait(lock);
//
class Manager {
 public:
  // INTERNAL USE. Manager is constructible from an implementation.
  Manager(ManagerPtr ptr) noexcept : ptr_(std::move(ptr)) {}

  // Manager is default constructible.  It begins in the empty state.
  Manager() noexcept : ptr_() {}

  // Manager is copyable and moveable.
  // - These copy and move the handle, not the implementation.
  //   A copy of a Manager is not a separate Manager, it is a new
  //   refcounted handle to the same Manager.
  Manager(const Manager&) noexcept = default;
  Manager(Manager&&) noexcept = default;
  Manager& operator=(const Manager&) noexcept = default;
  Manager& operator=(Manager&&) noexcept = default;

  // Resets this Manager to the empty state.
  void reset() noexcept { ptr_.reset(); }

  // Swaps this Manager with another.
  void swap(Manager& x) noexcept { ptr_.swap(x.ptr_); }

  // Returns true iff this Manager is non-empty.
  explicit operator bool() const noexcept { return !!ptr_; }

  // Asserts that this Manager is non-empty.
  void assert_valid() const noexcept;

  // Returns this Manager, if non-empty, or else returns the system_manager().
  Manager or_system_manager() const;

  // Returns this Manager's Poller implementation.
  PollerPtr poller() const noexcept {
    assert_valid();
    return ptr_->poller();
  }

  // Returns this Manager's Dispatcher implementation.
  DispatcherPtr dispatcher() const noexcept {
    assert_valid();
    return ptr_->dispatcher();
  }

  // Forwards the given <Task, Callback> to this Manager's Dispatcher.
  void dispatch(Task* /*nullable*/ task, CallbackPtr callback) const {
    dispatcher()->dispatch(task, std::move(callback));
  }
  void dispatch(CallbackPtr callback) const {
    dispatcher()->dispatch(std::move(callback));
  }

  // Convenience method for |dispatcher()->dispose(finalizer)|.
  void dispose(CallbackPtr finalizer) const {
    dispatcher()->dispose(std::move(finalizer));
  }

  // Convenience method for |dispatcher()->dispose(ptr)|.
  template <typename T>
  void dispose(T* ptr) const {
    dispatcher()->dispose(ptr);
  }

  // Registers an event handler for a file descriptor.
  base::Result fd(Handle* out, base::FD fd, Set set, HandlerPtr handler) const;

  // Registers an event handler for a Unix signal.
  base::Result signal(Handle* out, int signo, HandlerPtr handler) const;

  // Registers an event handler for a timer.
  // The timer is initially disarmed. Use |Handle::set_*()| to arm it.
  base::Result timer(Handle* out, HandlerPtr handler) const;

  // Registers an event handler for a generic event.
  base::Result generic(Handle* out, HandlerPtr handler) const;

  // Arranges for |task->expire()| to be called at time |at|.
  base::Result set_deadline(Task* task, base::MonotonicTime at);
  base::Result set_deadline(Task* task, base::Time at) {
    return set_deadline(task, base::system_monotonic_clock().convert(at));
  }

  // Arranges for |task->expire()| to be called after |delay|.
  base::Result set_timeout(Task* task, base::Duration delay);

  // Donates the current thread to the Manager, if supported.
  void donate(bool forever) const noexcept {
    assert_valid();
    ptr_->donate(forever);
  }

  // Shuts down the Manager and releases all resources.
  void shutdown() const noexcept {
    if (ptr_) ptr_->shutdown();
  }

 private:
  ManagerPtr ptr_;
};

inline void swap(Manager& a, Manager& b) noexcept { a.swap(b); }

// Constructs a new Manager instance.
base::Result new_manager(Manager* out, const ManagerOptions& opts);

// Returns a shared instance of Manager.
//
// THREAD SAFETY: This function is thread-safe.
//
Manager system_manager();

// Replaces the shared instance of Manager.
//
// THREAD SAFETY: This function is thread-safe.
//
void set_system_manager(Manager m);

// Blocks until at least |n| of the Tasks in |tv| have finished.
// PRECONDITION: |n <= tv.size()|
void wait_n(std::vector<Manager> mv, std::vector<Task*> tv, std::size_t n);

// Blocks until at least one of the Tasks in |tv| has finished.
inline void wait_any(std::vector<Manager> mv, std::vector<Task*> tv) {
  std::size_t n = tv.empty() ? 0 : 1;
  wait_n(std::move(mv), std::move(tv), n);
}

// Blocks until every Task in |tv| has finished.
inline void wait_all(std::vector<Manager> mv, std::vector<Task*> tv) {
  std::size_t n = tv.size();
  wait_n(std::move(mv), std::move(tv), n);
}

// Blocks until the given Task has finished.
inline void wait(Manager m, Task* task) { wait_n({m}, {task}, 1); }

inline Manager Manager::or_system_manager() const {
  if (ptr_)
    return *this;
  else
    return system_manager();
}

}  // namespace event

#endif  // EVENT_MANAGER_H
