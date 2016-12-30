// event/dispatcher.h - Running callbacks asynchronously
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include "base/result.h"
#include "event/callback.h"
#include "event/task.h"

namespace event {

class DispatcherOptions;  // forward declaration
struct DispatcherStats;   // forward declaration

// DispatcherType is used to identify which callback execution strategy to use.
enum class DispatcherType : uint8_t {
  // Create a new Dispatcher instance, but let the system pick which type.
  unspecified = 0,

  // Inline Dispatchers use the caller's thread to execute their callbacks.
  inline_dispatcher = 1,

  // Async Dispatchers collect callbacks into a work queue, then sit on it
  // until someone calls their |Dispatcher::donate(bool)| method.
  async_dispatcher = 2,

  // Threaded Dispatchers collect callbacks into a work queue, but have one or
  // more threads dedicated to processing the queue. They vary from a single
  // dispatcher thread to heuristically self-resizing thread pools.
  threaded_dispatcher = 3,

  // Instead of constructing a new Dispatcher instance, reuse the shared
  // Dispatcher instance provided by |system_dispatcher()|.
  system_dispatcher = 255,
};

// A Dispatcher is an implementation of a strategy for running callbacks.
//
// THREAD SAFETY: This class is thread-safe.
//
// REENTRANCY NOTE: Except for the exceptional methods noted below, it is safe
//                  to call Dispatcher methods from within a Callback.
//
class Dispatcher {
 protected:
  Dispatcher() noexcept = default;

 public:
  virtual ~Dispatcher() noexcept = default;

  // Dispatchers are neither copyable nor movable.
  Dispatcher(const Dispatcher&) = delete;
  Dispatcher(Dispatcher&&) = delete;
  Dispatcher& operator=(const Dispatcher&) = delete;
  Dispatcher& operator=(Dispatcher&&) = delete;

  // Returns the type of this Dispatcher.
  virtual DispatcherType type() const noexcept = 0;

  // Runs the provided Callback on the Dispatcher, with an optional Task.
  //
  // - If |task| is not provided, then |callback| cannot be cancelled and its
  //   return value will be discarded.
  //
  // - If |task| is provided and it is cancelled before the Dispatcher begins
  //   to execute |callback|, then |callback| will not be run at all.
  //
  // - If |task| is provided and |callback| runs, then |callback|'s
  //   base::Result return value will be stored in |task->finish()|.
  //
  virtual void dispatch(Task* /*nullable*/ task, CallbackPtr callback) = 0;

  // Runs the provided Callback on the Dispatcher.
  //
  // This is a convenience wrapper for |dispatch(nullptr, callback)|.
  // It is useful if:
  //  (a) you do not care about |callback|'s result;
  //  (b) you do not care about *when* |callback| completes; AND
  //  (c) you do not care about canceling |callback|.
  //
  void dispatch(CallbackPtr callback) {
    return dispatch(nullptr, std::move(callback));
  }

  // Runs the provided finalizer on the Dispatcher in a safe context.
  //
  // Unlike |dispatch()|, the Callback provided here may call |donate()| or
  // functions that can call it, such as |event::wait()|.
  //
  virtual void dispose(CallbackPtr finalizer) = 0;

  // Destroys the provided pointer on the Dispatcher in a safe context.
  //
  // |~T()| may call |donate()| or functions that can call it, such as
  // |event::wait()|.
  //
  template <typename T>
  void dispose(T* ptr) {
    auto closure = [ptr] {
      delete ptr;
      return base::Result();
    };
    dispose(event::callback(closure));
  }

  // Obtains statistics about this Dispatcher.
  virtual DispatcherStats stats() const noexcept = 0;

  // OPTIONAL. Changes the Dispatcher's options at runtime. May block.
  //
  // When the caller calls |adjust| after having previously invoked |shutdown|,
  // the implementation of |adjust| MUST have one of the following behaviors:
  //
  // - All call to |adjust| after |shutdown| will fail.
  // - The first call after |shutdown| will undo its effects, bringing this
  //   Dispatcher back to life.
  //
  // APPLIES: |threaded_dispatcher|
  //
  virtual base::Result adjust(const DispatcherOptions& opts) noexcept {
    return base::Result::not_implemented();
  }

  // OPTIONAL. Corks the Dispatcher, pausing the processing of callbacks.
  //
  // It is an error to call |cork()| on a corked Dispatcher.
  //
  // APPLIES: |threaded_dispatcher|
  //
  virtual void cork() noexcept {}

  // OPTIONAL. Uncorks the Dispatcher, resuming the processing of callbacks.
  //
  // It is an error to call |uncork()| on a Dispatcher that is not corked.
  //
  // APPLIES: |threaded_dispatcher|
  //
  virtual void uncork() noexcept {}

  // OPTIONAL. Donates the current thread to the Dispatcher, if supported.
  //
  // If this Dispatcher DOES support thread donation:
  //
  // - If |forever == false|, then Dispatcher may return control of the thread
  //   at certain times, such as when the work queue is empty. If the thing you
  //   were waiting for is ready, great! If it's not, feel free to do some
  //   housekeeping then call |donate| again.
  //
  // - If |forever == true|, then Dispatcher will not return control of the
  //   thread until |shutdown()| is called.
  //
  // If this Dispatcher DOES NOT support thread donation, returns immediately.
  //
  // APPLIES: |async_dispatcher|, |threaded_dispatcher|
  //
  // REENTRANCY NOTE: It is NEVER safe to call |donate| from within a Callback.
  //
  virtual void donate(bool forever) noexcept {}

  // Requests that the Dispatcher free all resources. Blocks.
  //
  // Depending on the implementation:
  // - Calls to |dispatch| MAY be ignored after this, or they may execute.
  // - Calls to |adjust| MAY undo this, bringing the Dispatcher back to life.
  //
  // Calls to |shutdown| are idempotent: two calls have the same effect as one.
  //
  virtual void shutdown() noexcept {}
};

// An IdleFunction is a function that Dispatcher instances may choose to call
// when they have nothing better to do.
//
// THREAD SAFETY: This function MUST be thread-safe.
using IdleFunction = std::function<void()>;

// A DispatcherOptions holds user-available choices in the selection and
// configuration of Dispatcher instances.
class DispatcherOptions {
 private:
  enum bits {
    bit_min = (1U << 0),
    bit_max = (1U << 1),
  };

 public:
  // DispatcherOptions is default constructible, copyable, and moveable.
  // There is intentionally no constructor for aggregate initialization.
  DispatcherOptions() noexcept : type_(DispatcherType::system_dispatcher),
                                 min_(0),
                                 max_(0),
                                 has_(0) {}
  DispatcherOptions(const DispatcherOptions&) = default;
  DispatcherOptions(DispatcherOptions&&) = default;
  DispatcherOptions& operator=(const DispatcherOptions&) = default;
  DispatcherOptions& operator=(DispatcherOptions&&) noexcept = default;

  // Resets all fields to their default values.
  void reset() noexcept { *this = DispatcherOptions(); }

  // The |idle_function()| value is a function object which certain Dispatcher
  // implementations may choose to invoke when they have nothing better to do,
  // i.e. when the work queue is empty and there are no threads to be
  // terminated.
  IdleFunction idle_function() const { return idle_; }
  void reset_idle_function() noexcept { idle_ = nullptr; }
  void set_idle_function(IdleFunction idle) noexcept {
    idle_ = std::move(idle);
  }

  // The |type()| value selects a Dispatcher implementation.
  DispatcherType type() const noexcept { return type_; }
  void reset_type() noexcept { type_ = DispatcherType::system_dispatcher; }
  void set_type(DispatcherType type) noexcept { type_ = type; }

  // The |min_workers()| value suggests a minimum number of worker threads.
  // - |min_workers().first| is true iff a custom value has been specified.
  // - |min_workers().second| is the custom value, if present.
  std::pair<bool, std::size_t> min_workers() const noexcept {
    return std::make_pair((has_ & bit_min) != 0, min_);
  }
  void reset_min_workers() noexcept {
    has_ &= ~bit_min;
    min_ = 0;
  }
  void set_min_workers(std::size_t min) noexcept {
    min_ = min;
    has_ |= bit_min;
  }

  // The |max_workers()| value suggests a maximum number of worker threads.
  // - |max_workers().first| is true iff a custom value has been specified.
  // - |max_workers().second| is the custom value, if present.
  std::pair<bool, std::size_t> max_workers() const noexcept {
    return std::make_pair((has_ & bit_max) != 0, max_);
  }
  void reset_max_workers() noexcept {
    has_ &= ~bit_max;
    max_ = 0;
  }
  void set_max_workers(std::size_t max) noexcept {
    max_ = max;
    has_ |= bit_max;
  }

  // Convenience methods for modifying both min_workers and max_workers.
  void reset_num_workers() noexcept {
    has_ &= ~(bit_min | bit_max);
    min_ = max_ = 0;
  }
  void set_num_workers(std::size_t min, std::size_t max) noexcept {
    min_ = min;
    max_ = max;
    has_ |= (bit_min | bit_max);
  }
  void set_num_workers(std::size_t n) noexcept { set_num_workers(n, n); }

 private:
  IdleFunction idle_;
  DispatcherType type_;
  std::size_t min_;
  std::size_t max_;
  uint8_t has_;
};

// A DispatcherStats holds statistics about a running Dispatcher instance.
// All fields are advisory only, as these statistics are only a snapshot.
struct DispatcherStats {
  // |min_workers| is the lower limit on the number of worker threads that this
  // instance will attempt to maintain.
  //
  // APPLIES: |threaded_dispatcher|
  //
  std::size_t min_workers;

  // |max_workers| is the upper limit on the number of worker threads that this
  // instance will attempt to maintain.
  //
  // APPLIES: |threaded_dispatcher|
  //
  std::size_t max_workers;

  // |desired_num_workers| is the number of worker threads which the instance
  // currently believes to be optimal for the workload.
  //
  // APPLIES: |threaded_dispatcher|
  //
  std::size_t desired_num_workers;

  // |current_num_workers| is the actual number of worker threads currently
  // maintained by the instance.
  //
  // APPLIES: |threaded_dispatcher|
  //
  std::size_t current_num_workers;

  // |pending_count| is the number of items in the work queue, i.e.  not yet
  // scheduled to a thread.
  //
  // APPLIES: |async_dispatcher|, |threaded_dispatcher|
  //
  std::size_t pending_count;

  // |active_count| is the number of callbacks currently executing.
  //
  // APPLIES: all
  //
  std::size_t active_count;

  // |completed_count| is the number of callbacks that have been executed.
  // This includes successful, failed, cancelled, and throwing callbacks.
  //
  // APPLIES: all
  //
  std::size_t completed_count;

  // |caught_exceptions| is the number of exceptions thrown by
  // Callbacks that were caught and discarded.
  //
  // APPLIES: all
  //
  std::size_t caught_exceptions;

  // |corked| is true iff the implementation is currently corked.
  //
  // APPLIES: |threaded_dispatcher|
  //
  bool corked;

  // DispatcherStats is default constructible, copyable, and moveable.
  // There is intentionally no constructor for aggregate initialization.
  DispatcherStats() noexcept : min_workers(0),
                               max_workers(0),
                               desired_num_workers(0),
                               current_num_workers(0),
                               pending_count(0),
                               active_count(0),
                               completed_count(0),
                               caught_exceptions(0),
                               corked(false) {}
  DispatcherStats(const DispatcherStats&) noexcept = default;
  DispatcherStats(DispatcherStats&&) noexcept = default;
  DispatcherStats& operator=(const DispatcherStats&) noexcept = default;
  DispatcherStats& operator=(DispatcherStats&&) noexcept = default;

  // Convenience method for the sum |pending_count + active_count|.
  std::size_t incomplete_count() const noexcept {
    return pending_count + active_count;
  }
};

using DispatcherPtr = std::shared_ptr<Dispatcher>;

// Constructs a new Dispatcher, or returns the shared |system_dispatcher()|, as
// specified in |opts|.
base::Result new_dispatcher(DispatcherPtr* out, const DispatcherOptions& opts);

// Returns a shared instance of an inline Dispatcher.
//
// THREAD SAFETY: This function is thread-safe.
//
DispatcherPtr system_inline_dispatcher();

// Returns a shared instance of a Dispatcher.
//
// THREAD SAFETY: This function is thread-safe.
//
DispatcherPtr system_dispatcher();

// Replaces the shared instance of Dispatcher.
//
// THREAD SAFETY: This function is thread-safe.
//
void set_system_dispatcher(DispatcherPtr ptr);

std::size_t num_cores();

namespace internal {
void assert_depth();
}  // namespace internal

}  // namespace event

#endif  // EVENT_DISPATCHER_H
