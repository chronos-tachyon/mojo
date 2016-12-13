// event/manager.h - Event-driven async or multithreaded I/O
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <functional>
#include <memory>
#include <vector>

#include "base/clock.h"
#include "base/duration.h"
#include "base/fd.h"
#include "base/result.h"
#include "base/time.h"
#include "base/token.h"
#include "event/data.h"
#include "event/dispatcher.h"
#include "event/handler.h"
#include "event/poller.h"
#include "event/set.h"
#include "event/task.h"

namespace event {

class ManagerImpl;  // forward declaration

// An event::FileDescriptor binds a file descriptor to an event::Manager.
class FileDescriptor {
 private:
  friend class Manager;

  FileDescriptor(std::shared_ptr<ManagerImpl> ptr, base::FD fd,
                 base::token_t t) noexcept : ptr_(std::move(ptr)),
                                             fd_(std::move(fd)),
                                             t_(t) {}

 public:
  // The default constructor produces an invalid (unbound) FileDescriptor.
  FileDescriptor() : ptr_(), fd_(), t_() {}

  // FileDescriptors are move-only objects.
  FileDescriptor(const FileDescriptor&) = delete;
  FileDescriptor(FileDescriptor&& x) noexcept = default;
  FileDescriptor& operator=(const FileDescriptor&) = delete;
  FileDescriptor& operator=(FileDescriptor&&) noexcept;

  // The destructor unbinds the Handler from the file descriptor.
  // NOTE: this does not *close* the file descriptor.
  ~FileDescriptor() noexcept { release().ignore_ok(); }

  explicit operator bool() const noexcept { return !!ptr_; }
  bool valid() const noexcept { return !!*this; }
  void assert_valid() const;

  void swap(FileDescriptor& x) noexcept {
    using std::swap;
    swap(ptr_, x.ptr_);
    swap(fd_, x.fd_);
    swap(t_, x.t_);
  }

  // Gets the set of events which the Handler is interested in.
  base::Result get(Set* out) const;

  // Replaces the set of events which the Handler is interested in.
  base::Result modify(Set set);

  // Unbinds the Handler from the file descriptor.
  // NOTE: this does NOT close the file descriptor.
  base::Result release();

 private:
  std::shared_ptr<ManagerImpl> ptr_;
  base::FD fd_;
  base::token_t t_;
};

inline void swap(FileDescriptor& a, FileDescriptor& b) noexcept { a.swap(b); }

// An event::Signal binds a Unix signal to an event::Manager.
class Signal {
 private:
  friend class Manager;

  Signal(std::shared_ptr<ManagerImpl> ptr, int signo, base::token_t t) noexcept
      : ptr_(std::move(ptr)),
        sig_(signo),
        t_(t) {}

 public:
  // The default constructor produces an invalid (unbound) Signal.
  Signal() noexcept : Signal(nullptr, -1, base::token_t()) {}

  // Signals are move-only objects.
  Signal(const Signal&) = delete;
  Signal(Signal&& x) noexcept : ptr_(std::move(x.ptr_)),
                                sig_(x.sig_),
                                t_(x.t_) {}
  Signal& operator=(const Signal&) = delete;
  Signal& operator=(Signal&&) noexcept;

  // The destructor unbinds the Handler from the signal.
  // NOTE: If this was the last Handler bound to the signal, then the default
  //       signal behavior is restored.
  ~Signal() noexcept { release().ignore_ok(); }

  explicit operator bool() const noexcept { return !!ptr_; }
  bool valid() const noexcept { return !!*this; }
  void assert_valid() const;

  void swap(Signal& x) noexcept {
    using std::swap;
    swap(ptr_, x.ptr_);
    swap(sig_, x.sig_);
    swap(t_, x.t_);
  }

  // Unbinds the Handler from the signal.
  // NOTE: If this was the last Handler bound to the signal, then the default
  //       signal behavior is restored.
  base::Result release();

 private:
  std::shared_ptr<ManagerImpl> ptr_;
  int sig_;
  base::token_t t_;
};

inline void swap(Signal& a, Signal& b) noexcept { a.swap(b); }

// An event::Timer binds a timer of some kind to an event::Manager.
// The timer is initially unarmed; use the Timer::set_* methods to arm.
class Timer {
 private:
  friend class Manager;

  Timer(std::shared_ptr<ManagerImpl> ptr, base::token_t t) noexcept
      : ptr_(std::move(ptr)),
        t_(t) {}

 public:
  // The default constructor produces an invalid (unbound) Timer.
  Timer() noexcept : Timer(nullptr, base::token_t()) {}

  // Timers are move-only objects.
  Timer(const Timer&) = delete;
  Timer(Timer&& x) noexcept : ptr_(std::move(x.ptr_)), t_(x.t_) {}
  Timer& operator=(const Timer&) = delete;
  Timer& operator=(Timer&&) noexcept;

  // The destructor unbinds the Handler from the timer and frees the timer.
  ~Timer() noexcept { release().ignore_ok(); }

  explicit operator bool() const noexcept { return !!ptr_; }
  bool valid() const noexcept { return !!*this; }
  void assert_valid() const;

  void swap(Timer& x) noexcept {
    using std::swap;
    swap(ptr_, x.ptr_);
    swap(t_, x.t_);
  }

  // Arms the timer as a one-shot timer for the given absolute time.
  // The time is specified in terms of the |base::system_monotonic_clock()|.
  base::Result set_at(base::MonotonicTime at);
  base::Result set_at(base::Time at) {
    return set_at(base::system_monotonic_clock().convert(at));
  }

  // Arms the timer as a one-shot timer for the given time (relative to now).
  base::Result set_delay(base::Duration delay);

  // Arms the timer as a periodic timer with the given period.
  base::Result set_periodic(base::Duration period);

  // Arms the timer as a periodic timer with the given period.
  // The first event will arrive at the given absolute time.
  // The time is specified in terms of the |base::system_monotonic_clock()|.
  base::Result set_periodic_at(base::Duration period, base::MonotonicTime at);
  base::Result set_periodic_at(base::Duration period, base::Time at) {
    return set_periodic_at(period, base::system_monotonic_clock().convert(at));
  }

  // Arms the timer as a periodic timer with the given period.
  // The first event will arrive at the given time (relative to now).
  base::Result set_periodic_delay(base::Duration period, base::Duration delay);

  // Disarms the timer. The Timer remains valid, but it produces no further
  // events until it is armed again.
  base::Result cancel();

  // Unbinds the Handler from the timer and frees the timer.
  base::Result release();

 private:
  std::shared_ptr<ManagerImpl> ptr_;
  base::token_t t_;
};

inline void swap(Timer& a, Timer& b) noexcept { a.swap(b); }

// An event::Generic binds an arbitrary event to an event::Manager.
class Generic {
 private:
  friend class Manager;

  Generic(std::shared_ptr<ManagerImpl> ptr, base::token_t t) noexcept
      : ptr_(std::move(ptr)),
        t_(t) {}

 public:
  // The default constructor produces an invalid (unbound) Generic.
  Generic() noexcept : Generic(nullptr, base::token_t()) {}

  // Generics are move-only objects.
  Generic(const Generic&) = delete;
  Generic(Generic&& x) noexcept : ptr_(std::move(x.ptr_)), t_(x.t_) {}
  Generic& operator=(const Generic&) = delete;
  Generic& operator=(Generic&&) noexcept;

  // The destructor unbinds the Handler from the event and frees any resources.
  ~Generic() noexcept { release().ignore_ok(); }

  explicit operator bool() const noexcept { return !!ptr_; }
  bool valid() const noexcept { return !!*this; }
  void assert_valid() const;

  void swap(Generic& x) noexcept {
    using std::swap;
    swap(ptr_, x.ptr_);
    swap(t_, x.t_);
  }

  // Fires the event, triggering the associated Handler.
  // |value| will be available as |data.int_value| in the Handler.
  base::Result fire(int value = 0) const;

  // Unbinds the Handler from the event and frees any resources.
  base::Result release();

 private:
  std::shared_ptr<ManagerImpl> ptr_;
  base::token_t t_;
};

inline void swap(Generic& a, Generic& b) noexcept { a.swap(b); }

// A ManagerOptions holds user-available choices in the configuration of
// Manager instances.
class ManagerOptions {
 private:
  enum bits {
    bit_min = (1U << 0),
    bit_max = (1U << 1),
  };

 public:
  // ManagerOptions is default constructible, copyable, and moveable.
  // There is intentionally no constructor for aggregate initialization.
  ManagerOptions() noexcept : min_(0), max_(0), has_(0) {}
  ManagerOptions(const ManagerOptions&) = default;
  ManagerOptions(ManagerOptions&&) noexcept = default;
  ManagerOptions& operator=(const ManagerOptions&) = default;
  ManagerOptions& operator=(ManagerOptions&&) noexcept = default;

  // Resets all fields to their default values.
  void reset() { *this = ManagerOptions(); }

  // Options for configuring the Poller instance.
  PollerOptions& poller() noexcept { return poller_; }
  const PollerOptions& poller() const noexcept { return poller_; }

  // Options for configuring the Dispatcher instance.
  DispatcherOptions& dispatcher() noexcept { return dispatcher_; }
  const DispatcherOptions& dispatcher() const noexcept { return dispatcher_; }

  // The |min_pollers()| value suggests a minimum number of polling threads.
  // - |min_pollers().first| is true iff a custom value has been specified.
  // - |min_pollers().second| is the custom value, if present.
  std::pair<bool, std::size_t> min_pollers() const {
    return std::make_pair((has_ & bit_min) != 0, min_);
  }
  void reset_min_pollers() noexcept {
    has_ &= ~bit_min;
    min_ = 0;
  }
  void set_min_pollers(std::size_t min) noexcept {
    min_ = min;
    has_ |= bit_min;
  }

  // The |max_pollers()| value suggests a maximum number of polling threads.
  // - |max_pollers().first| is true iff a custom value has been specified.
  // - |max_pollers().second| is the custom value, if present.
  std::pair<bool, std::size_t> max_pollers() const {
    return std::make_pair((has_ & bit_max) != 0, max_);
  }
  void reset_max_pollers() noexcept {
    has_ &= ~bit_max;
    max_ = 0;
  }
  void set_max_pollers(std::size_t max) noexcept {
    max_ = max;
    has_ |= bit_max;
  }

  // Convenience methods for modifying both min_pollers and max_pollers.
  void reset_num_pollers() noexcept {
    has_ &= ~(bit_min | bit_max);
    min_ = max_ = 0;
  }
  void set_num_pollers(std::size_t min, std::size_t max) noexcept {
    min_ = min;
    max_ = max;
    has_ |= (bit_min | bit_max);
  }
  void set_num_pollers(std::size_t n) noexcept { set_num_pollers(n, n); }

 private:
  PollerOptions poller_;
  DispatcherOptions dispatcher_;
  std::size_t min_;
  std::size_t max_;
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
//    event::Timer timer;
//    result = m.timer(&timer, event::handler([&] (event::Data data) {
//      std::unique_lock<std::mutex> lock(mu);
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
  Manager(std::shared_ptr<ManagerImpl> ptr) noexcept : ptr_(std::move(ptr)) {}
  Manager() noexcept : ptr_() {}

  // Managers are copyable and moveable.
  Manager(const Manager&) noexcept = default;
  Manager(Manager&&) noexcept = default;
  Manager& operator=(const Manager&) noexcept = default;
  Manager& operator=(Manager&&) noexcept = default;

  void reset() noexcept { ptr_.reset(); }
  void swap(Manager& x) noexcept { ptr_.swap(x.ptr_); }
  explicit operator bool() const noexcept { return !!ptr_; }
  bool valid() const noexcept { return !!*this; }
  void assert_valid() const;

  // Returns this Manager, if valid, or else returns the system_manager().
  Manager& or_system_manager();
  const Manager& or_system_manager() const;

  // Returns this Manager's Poller implementation.
  std::shared_ptr<Poller> poller() const;

  // Returns this Manager's Dispatcher implementation.
  std::shared_ptr<Dispatcher> dispatcher() const;

  // Forwards the given <Task, Callback> to this Manager's Dispatcher.
  void dispatch(Task* task, std::unique_ptr<Callback> callback) const {
    dispatcher()->dispatch(task, std::move(callback));
  }
  void dispatch(std::unique_ptr<Callback> callback) const {
    dispatcher()->dispatch(std::move(callback));
  }

  // Registers an event handler for a file descriptor.
  base::Result fd(FileDescriptor* out, base::FD fd, Set set,
                  std::shared_ptr<Handler> handler) const;

  // Registers an event handler for a Unix signal.
  base::Result signal(Signal* out, int signo,
                      std::shared_ptr<Handler> handler) const;

  // Registers an event handler for a timer.
  // The timer is initially disarmed. Use |Timer::set_*()| to arm it.
  base::Result timer(Timer* out, std::shared_ptr<Handler> handler) const;

  // Registers an event handler for a generic event.
  base::Result generic(Generic* out, std::shared_ptr<Handler> handler) const;

  // Arranges for |task->expire()| to be called at time |at|.
  base::Result set_deadline(Task* task, base::MonotonicTime at);
  base::Result set_deadline(Task* task, base::Time at) {
    return set_deadline(task, base::system_monotonic_clock().convert(at));
  }

  // Arranges for |task->expire()| to be called after |delay|.
  base::Result set_timeout(Task* task, base::Duration delay);

  // Donates the current thread to the Manager, if supported.
  base::Result donate(bool forever) const;

  // Shuts down the Manager and releases all resources.
  base::Result shutdown() const;

 private:
  std::shared_ptr<ManagerImpl> ptr_;
};

inline void swap(Manager& a, Manager& b) noexcept { a.swap(b); }

// Constructs a new Manager instance.
base::Result new_manager(Manager* out, const ManagerOptions& opts);

// Returns a shared instance of Manager.
//
// THREAD SAFETY: This function is thread-safe.
//
Manager& system_manager();

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

inline Manager& Manager::or_system_manager() {
  if (ptr_)
    return *this;
  else
    return system_manager();
}

inline const Manager& Manager::or_system_manager() const {
  if (ptr_)
    return *this;
  else
    return system_manager();
}

}  // namespace event

#endif  // EVENT_MANAGER_H
