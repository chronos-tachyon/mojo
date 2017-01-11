// event/task.h - Asynchronous function results
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef EVENT_TASK_H
#define EVENT_TASK_H

#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <ostream>
#include <vector>

#include "base/mutex.h"
#include "base/result.h"
#include "event/callback.h"

namespace event {

class Dispatcher;  // forward declaration

// Enumeration of the possible states of a Task.
enum class TaskState : uint8_t {
  // The Task has not yet started.
  //
  // NEXT STATES: |running|, |done|
  ready = 0,

  // The Task is currently running, its deadline has not expired, and it has
  // not been cancelled.
  //
  // NEXT STATES: |expiring|, |cancelling|, |done|
  running = 1,

  // The Task is currently running, but it has exceeded its deadline.
  // It SHOULD acknowledge the expiration, but it MAY run to completion.
  //
  // NEXT STATES: |cancelling|, |done|
  expiring = 2,

  // The Task is currently running, but it has been cancelled.
  // It SHOULD acknowledge the cancellation, but it MAY run to completion.
  //
  // NEXT STATES: |done|
  cancelling = 3,

  // The Task has completed.
  // This does not mean it was successful: check its outcome with |result()|.
  //
  // NEXT STATES: N/A (terminal)
  done = 4,
};

void append_to(std::string* out, TaskState state);
std::size_t length_hint(TaskState state) noexcept;

inline std::ostream& operator<<(std::ostream& o, TaskState state) {
  std::string str;
  append_to(&str, state);
  return (o << str);
}

// A Task is used by asynchronous and/or threaded functions as an output
// parameter for returning a base::Result, with the side effect of notifying
// the caller of completion in the process. Task is commonly used in
// conjunction with an event::Manager.
//
// Task supports deadlines. Deadlines can be used to abort operations that get
// stuck waiting on a resource, e.g. because a networked host is down. Task
// deadlines are strictly advisory: the operation must check for them.
//
// - To set a deadline, call |event::Manager::set_deadline()|. The Manager will
//   arrange for |Task::expire()| to be called when the deadline expires.
//
// Task also supports asynchronous cancellation: the caller can arrange for
// |Task::cancel()| to be called, and the asynchronous callee can observe this
// request and cancel the long-running operation.
//
// Compare and contrast with |std::future|: |std::future| supports arbitrary
// value types, but has no concept either of deadlines or of cancellation.
//
// THREAD SAFETY: This class is thread-safe.
//
class Task {
 public:
  using State = TaskState;
  using DispatcherPtr = std::shared_ptr<Dispatcher>;

  struct Work {
    DispatcherPtr dispatcher;
    CallbackPtr callback;

    Work(DispatcherPtr d, CallbackPtr c) noexcept : dispatcher(std::move(d)),
                                                    callback(std::move(c)) {}
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

  // Returns the current state of the Task.
  State state() const noexcept {
    auto lock = base::acquire_lock(mu_);
    return state_;
  }

  // Returns true iff the Task is in the |running| state, i.e. it has been
  // started, its deadline has not expired, AND it has not been cancelled.
  //
  // Typical usage:
  //
  //    while (!done) {
  //      if (!task->is_running()) {
  //        task->finish_cancel();
  //        return;
  //      }
  //      ...;  // incremental long-running operation
  //    }
  //
  bool is_running() const noexcept { return state() == State::running; }

  // Returns true iff the Task is in the terminal state, |done|.
  bool is_finished() const noexcept { return state() >= State::done; }

  // Returns the result of the Task.
  // - If the Task finished with an exception, rethrows the exception.
  // PRECONDITION: |is_finished() == true|.
  base::Result result() const;

  // Returns true if |result()| will throw an exception.
  bool result_will_throw() const noexcept;

  // Registers another Task as a subtask of this Task.
  // - If this Task reaches |expiring|, |cancelling|, or |done|, then all
  //   subtasks will be cancelled.
  // - Will cancel |task| immediately if this Task is already |done|.
  void add_subtask(Task* task);

  // Registers a Callback to execute when the Task reaches |expiring|,
  // |cancelling|, or |done| with result DEADLINE_EXCEEDED or CANCELLED.
  // - Will execute |callback| immediately if this Task is already cancelled
  void on_cancelled(DispatcherPtr /*nullable*/ dispatcher,
                    CallbackPtr callback);
  void on_cancelled(CallbackPtr callback) {
    on_cancelled(nullptr, std::move(callback));
  }

  // Registers a Callback to execute when the Task reaches the |done| state.
  // - Will execute |callback| immediately if this Task is already |done|.
  void on_finished(DispatcherPtr /*nullable*/ dispatcher, CallbackPtr callback);
  void on_finished(CallbackPtr callback) {
    on_finished(nullptr, std::move(callback));
  }

  // Marks the task as having exceeded its deadline.
  // - Changes |ready| to |done| with result DEADLINE_EXCEEDED and returns true
  // - Changes |running| to |expiring| and returns false
  // - Has no effect otherwise (and returns false)
  //
  // This is normally called via |event::Manager::set_deadline()| and friends.
  //
  bool expire() noexcept;

  // Requests that the Task be cancelled.
  // - Changes |ready| to |done| with result CANCELLED and returns true
  // - Changes |running| to |cancelling| and returns false
  // - Changes |expiring| to |cancelling| and returns false
  // - Has no effect otherwise (and returns false)
  bool cancel() noexcept;

  // Marks the Task as running, unless the Task was already cancelled.
  // - Changes |ready| to |running| and returns true
  // - Has no effect if state was |done| (and returns false)
  // PRECONDITION: state is |ready| or |done|
  bool start();

  // Marks the task as finished with a result.
  // - Changes |running| to |done| and returns true
  // - Changes |expiring| to |done| and returns true
  // - Changes |cancelling| to |done| and returns true
  // - Has no effect if the state is already |done| (and returns false)
  // PRECONDITION: state is not |ready|
  bool finish(base::Result result);

  // Convenience method for finishing with an OK result.
  // Return values and preconditions are the same as for |finish()|.
  bool finish_ok() { return finish(base::Result()); }

  // Convenience method for finishing with DEADLINE_EXCEEDED or CANCELLED.
  // Return values and preconditions are the same as for |finish()|.
  bool finish_cancel();

  // Marks the task as finished with an exception.
  // Return values and preconditions are the same as for |finish()|.
  bool finish_exception(std::exception_ptr eptr);

 private:
  static base::Result incomplete_result();
  static base::Result exception_result();

  bool cancel_impl(State next, base::Result result) noexcept;
  void finish_impl(std::unique_lock<std::mutex> lock, base::Result result,
                   std::exception_ptr eptr);

  mutable std::mutex mu_;
  State state_;
  base::Result result_;
  std::exception_ptr eptr_;
  std::vector<Work> on_finish_;
  std::vector<Work> on_cancel_;
  std::vector<Task*> subtasks_;
};

}  // namespace event

#endif  // EVENT_TASK_H
