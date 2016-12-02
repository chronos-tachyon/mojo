// event/task.h - Asynchronous function results
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef EVENT_TASK_H
#define EVENT_TASK_H

#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>

#include "base/result.h"
#include "event/callback.h"

namespace event {

// A Task is used by asynchronous and/or threaded functions as an output
// parameter for returning a base::Result, with the side effect of notifying
// the caller of completion in the process.
//
// Task also supports asynchronous cancellation: the caller can arrange for
// |Task::cancel()| to be called, and the asynchronous callee can observe this
// request and cancel the long-running operation.
//
// Task is commonly used in conjunction with event::Manager.
//
// THREAD SAFETY: This class is thread-safe.
//
class Task {
 public:
  // The state of a Task must be one of these.
  enum class State : uint8_t {
    // The Task has not yet started.
    //
    // NEXT STATES: |running|, |finishing|
    ready = 0,

    // The Task is currently running and has not been cancelled.
    //
    // NEXT STATES: |cancelling|, |finishing|
    running = 1,

    // The Task is currently running, but it has been cancelled.
    // The task MAY run to completion (state |done|), or it MAY acknowledge the
    // cancellation and stop itself before completion (state |cancelled|).
    //
    // NEXT STATES: |finishing|
    cancelling = 2,

    // The Task is currently calling |finish()|.
    //
    // NEXT STATES: |done|, |cancelled|
    finishing = 3,

    // The Task has completed normally.
    // This does not mean it was successful: check its outcome with |result()|.
    //
    // NEXT STATES: N/A (terminal)
    done = 4,

    // The Task was cancelled before it could complete normally.
    // |result().code()| will be base::Result::Code::CANCELLED.
    //
    // NEXT STATES: N/A (terminal)
    cancelled = 5,
  };

  // Constructs an empty Task, ready for use.
  Task() : state_(State::ready), result_(incomplete_result()) {}

  // Tasks are neither copyable nor movable.
  Task(const Task&) = delete;
  Task(Task&&) = delete;
  Task& operator=(const Task&) = delete;
  Task& operator=(Task&&) = delete;

  // Returns the Task to its initial state, ready to be reused.
  //
  // NOTE: This method must be used with extreme care. The caller must ensure
  //       that no other threads still have references to this Task object.
  void reset();

  // Returns an atomic snapshot of the current state of the Task.
  State state() const noexcept {
    auto lock = acquire_lock();
    return state_;
  }

  // Returns true iff the Task is being cancelled.
  bool is_cancelling() const noexcept { return state() == State::cancelling; }

  // Returns true iff the Task is in a terminal state: |done| or |cancelled|.
  bool is_finished() const noexcept { return state() >= State::done; }

  // Returns the result of the Task.
  // - If the Task finished with an exception, rethrows the exception.
  // PRECONDITION: |is_finished() == true|.
  base::Result result() const;

  // Registers another Task as a subtask of this Task.
  // If this Task is cancelled, then all subtasks will also be cancelled.
  void add_subtask(Task* task);

  // Registers a Callback to execute when the Task reaches a terminal state.
  void on_finished(std::unique_ptr<Callback> callback);

  // Requests that the Task be cancelled.
  // - Returns true iff the state changed from |ready| to |cancelled|
  // - Returns false if the state changed from |running| to |cancelling|
  // - Returns false if the state had any other value
  bool cancel() noexcept;

  // Marks the Task as running.
  // - Returns true if the state changed from |ready| to |running|
  // - Returns false if the state was |cancelled|
  // PRECONDITION: state is |ready| or |cancelled|
  bool start();

  // Marks the task as finished with a result.
  // - Returns true if the state changed from |running| to |done|
  // - Returns true if the state changed from |cancelling| to |done|
  // - Returns false if the state did not change
  // PRECONDITION: state is not |ready|
  bool finish(base::Result result);

  // Convenience method for finishing with an OK result.
  // Return values and preconditions are the same as for |finish()|.
  bool finish_ok() { return finish(base::Result()); }

  // Convenience method for finishing with a CANCELLED result.
  // Return values and preconditions are the same as for |finish()|.
  bool finish_cancel() { return finish(base::Result::cancelled()); }

  // Marks the task as finished with an exception.
  // Return values and preconditions are the same as for |finish()|.
  bool finish_exception(std::exception_ptr eptr);

 private:
  static base::Result incomplete_result();
  static base::Result exception_result();

  std::unique_lock<std::mutex> acquire_lock() const {
    return std::unique_lock<std::mutex>(mu_);
  }

  void finish_impl(std::unique_lock<std::mutex> lock, base::Result result,
                   std::exception_ptr eptr);

  mutable std::mutex mu_;
  State state_;
  base::Result result_;
  std::exception_ptr eptr_;
  std::vector<std::unique_ptr<Callback>> callbacks_;
  std::vector<Task*> subtasks_;
};

}  // namespace event

#endif  // EVENT_TASK_H
