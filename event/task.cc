// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/task.h"

#include <stdexcept>

#include "base/logging.h"
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
  auto lock = base::acquire_lock(mu_);
  switch (state_) {
    case State::ready:
    case State::done:
      result_ = incomplete_result();
      eptr_ = nullptr;
      callbacks_.clear();
      subtasks_.clear();
      state_ = State::ready;
      break;

    default:
      LOG(DFATAL) << "BUG: event::Task: reset() on running task!";
  }
}

static void assert_finished(Task::State state) {
  if (state < Task::State::done) {
    LOG(DFATAL) << "BUG: event::Task is not yet finished!";
  }
}

base::Result Task::result() const {
  auto lock = base::acquire_lock(mu_);
  assert_finished(state_);
  if (eptr_) std::rethrow_exception(eptr_);
  return result_;
}

bool Task::result_will_throw() const noexcept {
  auto lock = base::acquire_lock(mu_);
  assert_finished(state_);
  if (eptr_) return true;
  return false;
}

void Task::add_subtask(Task* subtask) {
  auto lock = base::acquire_lock(mu_);
  if (state_ > State::running) {
    lock.unlock();
    subtask->cancel();
  } else {
    // OPTIMIZATION: It's a common pattern to reset and reuse the same
    //               subtask multiple times. If |subtask| was already the
    //               most recently added subtask, don't add it twice.
    if (subtasks_.empty() || subtasks_.back() != subtask)
      subtasks_.push_back(subtask);
  }
}

void Task::on_finished(std::unique_ptr<Callback> cb) {
  auto lock = base::acquire_lock(mu_);
  if (state_ >= State::done) {
    lock.unlock();
    system_inline_dispatcher()->dispatch(nullptr, std::move(cb));
  } else {
    callbacks_.push_back(std::move(cb));
  }
}

bool Task::expire() noexcept {
  return cancel_impl(State::expiring, base::Result::deadline_exceeded());
}

bool Task::cancel() noexcept {
  return cancel_impl(State::cancelling, base::Result::cancelled());
}

bool Task::cancel_impl(State next, base::Result result) noexcept {
  auto lock = base::acquire_lock(mu_);
  std::vector<Task*> subtasks;
  if (state_ == State::ready) {
    finish_impl(std::move(lock), std::move(result), nullptr);
    return true;
  }
  if (state_ >= State::running && state_ < next) {
    state_ = next;
    subtasks = std::move(subtasks_);
    lock.unlock();
    for (Task* subtask : subtasks) {
      subtask->cancel();
    }
  }
  return false;
}

bool Task::start() {
  auto lock = base::acquire_lock(mu_);
  if (state_ == State::ready) {
    state_ = State::running;
    return true;
  }
  if (state_ < State::done) {
    LOG(DFATAL) << "BUG: event::Task: start() on running task!";
  }
  return false;
}

bool Task::finish(base::Result result) {
  auto lock = base::acquire_lock(mu_);
  if (state_ < State::running) {
    LOG(DFATAL) << "BUG: event::Task: finish() without start()!";
    state_ = State::running;
  }
  if (state_ < State::done) {
    finish_impl(std::move(lock), std::move(result), nullptr);
    return true;
  }
  return false;
}

bool Task::finish_cancel() {
  auto lock = base::acquire_lock(mu_);
  if (state_ < State::running) {
    LOG(DFATAL) << "BUG: event::Task: finish_cancel() without start()";
    state_ = State::running;
  }
  if (state_ < State::done) {
    auto r = base::Result::cancelled();
    if (state_ == State::expiring) r = base::Result::deadline_exceeded();
    finish_impl(std::move(lock), std::move(r), nullptr);
    return true;
  }
  return false;
}

bool Task::finish_exception(std::exception_ptr eptr) {
  auto lock = base::acquire_lock(mu_);
  if (state_ < State::running) {
    LOG(DFATAL) << "BUG: event::Task: finish_exception() without start()";
    state_ = State::running;
  }
  if (state_ < State::done) {
    finish_impl(std::move(lock), exception_result(), eptr);
    return true;
  }
  return false;
}

void Task::finish_impl(std::unique_lock<std::mutex> lock, base::Result result,
                       std::exception_ptr eptr) {
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
