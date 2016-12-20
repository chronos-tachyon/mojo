// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/task.h"

#include <stdexcept>

#include "base/logging.h"
#include "event/dispatcher.h"

using RC = base::Result::Code;

static const char* const kTaskStateNames[] = {
    "ready",      "running",    "expiring",   "cancelling", "reserved#4",
    "reserved#5", "reserved#6", "reserved#7", "done",
};

namespace event {

static void assert_finished(Task::State state) {
  CHECK_GE(state, Task::State::done) << ": event::Task is not yet finished!";
}

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
  if (state_ == State::ready) return;
  assert_finished(state_);
  result_ = incomplete_result();
  eptr_ = nullptr;
  on_finish_.clear();
  on_cancel_.clear();
  subtasks_.clear();
  state_ = State::ready;
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

void Task::on_cancelled(CallbackPtr cb) {
  auto lock = base::acquire_lock(mu_);
  bool run;
  if (state_ >= State::done) {
    RC rc = result_.code();
    run = (rc == RC::DEADLINE_EXCEEDED || rc == RC::CANCELLED);
  } else if (state_ > State::running) {
    run = true;
  } else {
    run = false;
    on_cancel_.push_back(std::move(cb));
  }
  if (run) {
    lock.unlock();
    system_inline_dispatcher()->dispatch(nullptr, std::move(cb));
  }
}

void Task::on_finished(CallbackPtr cb) {
  auto lock = base::acquire_lock(mu_);
  if (state_ >= State::done) {
    lock.unlock();
    system_inline_dispatcher()->dispatch(nullptr, std::move(cb));
  } else {
    on_finish_.push_back(std::move(cb));
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
  if (state_ == State::ready) {
    finish_impl(std::move(lock), std::move(result), nullptr);
    return true;
  }
  if (state_ >= State::running && state_ < next) {
    state_ = next;
    auto on_cancel = std::move(on_cancel_);
    auto subtasks = std::move(subtasks_);
    lock.unlock();

    auto d = system_inline_dispatcher();
    for (auto& cb : on_cancel) {
      d->dispatch(nullptr, std::move(cb));
    }

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
  CHECK_GE(state_, State::done) << ": event::Task: start() on running task!";
  return false;
}

bool Task::finish(base::Result result) {
  auto lock = base::acquire_lock(mu_);
  CHECK_GE(state_, State::running)
      << ": event::Task: finish() without start()!";
  if (state_ < State::done) {
    finish_impl(std::move(lock), std::move(result), nullptr);
    return true;
  }
  return false;
}

bool Task::finish_cancel() {
  auto lock = base::acquire_lock(mu_);
  CHECK_GE(state_, State::running)
      << ": event::Task: finish_cancel() without start()";
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
  CHECK_GE(state_, State::running)
      << ": event::Task: finish_exception() without start()";
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
  auto on_finish = std::move(on_finish_);
  auto on_cancel = std::move(on_cancel_);
  auto subtasks = std::move(subtasks_);
  lock.unlock();

  auto d = system_inline_dispatcher();
  RC rc = result_.code();
  if (rc == RC::DEADLINE_EXCEEDED || rc == RC::CANCELLED) {
    for (auto& cb : on_cancel) {
      d->dispatch(nullptr, std::move(cb));
    }
  }

  for (Task* subtask : subtasks) {
    subtask->cancel();
  }

  for (auto& cb : on_finish) {
    d->dispatch(nullptr, std::move(cb));
  }
}

std::ostream& operator<<(std::ostream& o, Task::State state) {
  return (o << kTaskStateNames[static_cast<uint8_t>(state)]);
}

}  // namespace event
