// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/task.h"

#include <stdexcept>

#include "event/dispatcher.h"

namespace event {

base::Result Task::incomplete_result() {
  return base::Result::internal(
      "BUG: this Task hasn't finished yet; how did you see this?");
}

base::Result Task::exception_result() {
  return base::Result::internal(
      "BUG: this Task finished with an exception; how did you see this?");
}

void Task::reset() {
  auto lock = acquire_lock();
  switch (state_) {
    case State::ready:
    case State::done:
    case State::cancelled:
      result_ = incomplete_result();
      eptr_ = nullptr;
      callbacks_.clear();
      subtasks_.clear();
      state_ = State::ready;
      break;

    default:
      throw std::runtime_error(
          "attempt to reset an event::Task that is currently live!");
  }
}

static void assert_finished(Task::State state) {
  switch (state) {
    case Task::State::done:
    case Task::State::cancelled:
      break;

    default:
      throw std::logic_error("event::Task is not yet finished");
  }
}

base::Result Task::result() const {
  auto lock = acquire_lock();
  assert_finished(state_);
  if (eptr_) std::rethrow_exception(eptr_);
  return result_;
}

void Task::add_subtask(Task* subtask) {
  auto lock = acquire_lock();
  switch (state_) {
    case State::cancelling:
    case State::done:
    case State::cancelled:
      lock.unlock();
      subtask->cancel();
      break;

    default:
      // OPTIMIZATION: It's a common pattern to reset and reuse the same
      //               subtask multiple times. If |subtask| was already the
      //               most recently added subtask, don't add it twice.
      if (subtasks_.empty() || subtasks_.back() != subtask)
        subtasks_.push_back(subtask);
  }
}

void Task::on_finished(std::unique_ptr<Callback> cb) {
  auto lock = acquire_lock();
  switch (state_) {
    case State::done:
    case State::cancelled:
      lock.unlock();
      system_inline_dispatcher()->dispatch(nullptr, std::move(cb));
      return;

    default:
      callbacks_.push_back(std::move(cb));
  }
}

bool Task::cancel() noexcept {
  auto lock = acquire_lock();
  std::vector<Task*> subtasks;
  switch (state_) {
    case State::ready:
      state_ = State::finishing;
      finish_impl(std::move(lock), base::Result::cancelled(), nullptr);
      return true;

    case State::running:
      state_ = State::cancelling;
      subtasks = std::move(subtasks_);
      lock.unlock();
      for (Task* subtask : subtasks) {
        subtask->cancel();
      }
      return false;

    default:
      return false;
  }
}

bool Task::start() {
  auto lock = acquire_lock();
  switch (state_) {
    case State::ready:
      state_ = State::running;
      return true;

    case State::cancelled:
      return false;

    case State::running:
    case State::cancelling:
      throw std::logic_error("event::Task: start() on running task");

    default:
      throw std::logic_error("event::Task: start() on finished task");
  }
}

bool Task::finish(base::Result result) {
  auto lock = acquire_lock();
  switch (state_) {
    case State::ready:
      throw std::logic_error("event::Task: finish() without start()");

    case State::running:
    case State::cancelling:
      finish_impl(std::move(lock), std::move(result), nullptr);
      return true;

    default:
      return false;
  }
}

bool Task::finish_exception(std::exception_ptr eptr) {
  auto lock = acquire_lock();
  switch (state_) {
    case State::ready:
      throw std::logic_error("event::Task: finish() without start()");

    case State::running:
    case State::cancelling:
      finish_impl(std::move(lock), exception_result(), eptr);
      return true;

    default:
      return false;
  }
}

void Task::finish_impl(std::unique_lock<std::mutex> lock, base::Result result,
                       std::exception_ptr eptr) {
  if (result.code() == base::Result::Code::CANCELLED)
    state_ = State::cancelled;
  else
    state_ = State::done;
  result_ = std::move(result);
  eptr_ = eptr;
  auto subtasks = std::move(subtasks_);
  auto callbacks = std::move(callbacks_);
  lock.unlock();

  for (Task* subtask : subtasks) {
    subtask->cancel();
  }

  auto d = system_inline_dispatcher();
  for (auto& cb : callbacks) {
    d->dispatch(nullptr, std::move(cb));
  }
}

}  // namespace event
