// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/task.h"

#include <exception>
#include <stdexcept>

#include "base/logging.h"
#include "event/dispatcher.h"

using RC = base::ResultCode;
using Work = event::internal::TaskWork;
using State = event::TaskState;

static const char* const kTaskStateNames[] = {
    "ready", "running", "expiring", "cancelling", "unstarted", "done",
};

namespace event {

static void invoke(Work work) noexcept {
  try {
    if (work.dispatcher) {
      work.dispatcher->dispatch(nullptr, std::move(work.callback));
    } else {
      work.callback->run();
    }
  } catch (...) {
    LOG_EXCEPTION(std::current_exception());
  }
}

static void lifo_callbacks(std::vector<Work> vec) noexcept {
  while (!vec.empty()) {
    auto work = std::move(vec.back());
    vec.pop_back();
    invoke(std::move(work));
  }
}

static void lifo_subtasks(std::vector<Task*> vec) noexcept {
  while (!vec.empty()) {
    Task* subtask = vec.back();
    vec.pop_back();
    subtask->cancel();
  }
}

static void assert_destructible(State state) noexcept {
  switch (state) {
    case State::ready:
    case State::done:
      break;

    default:
      LOG(DFATAL) << "BUG! event::Task is neither ready nor done";
  }
}

static void assert_running(State state) noexcept {
  switch (state) {
    case State::running:
    case State::expiring:
    case State::cancelling:
      break;

    default:
      LOG(DFATAL) << "BUG! event::Task is not running";
  }
}

static void assert_finished(State state) noexcept {
  switch (state) {
    case State::done:
      break;

    default:
      LOG(DFATAL) << "BUG! event::Task is not done";
  }
}

void append_to(std::string* out, State state) {
  CHECK_NOTNULL(out);
  out->append(kTaskStateNames[static_cast<uint8_t>(state)]);
}

std::size_t length_hint(State) noexcept { return 10; }

base::Result Task::incomplete_result() {
  return base::Result::internal(
      "BUG: this Task hasn't finished yet; how did you see this?");
}

base::Result Task::exception_result() {
  return base::Result::internal(
      "BUG: this Task finished with an exception; how did you see this?");
}

Task::~Task() noexcept {
  auto lock = base::acquire_lock(mu_);
  assert_destructible(state_);
}

void Task::reset() {
  auto lock = base::acquire_lock(mu_);
  assert_destructible(state_);
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
  bool run = false;
  switch (state_) {
    case State::ready:
      LOG(DFATAL) << "BUG! event::Task is not started";
      break;

    case State::running:
      break;

    case State::expiring:
    case State::cancelling:
    case State::done:
      run = true;
      break;

    case State::unstarted:
      LOG(DFATAL) << "BUG! event::Task is not started";
      run = true;
      break;
  }
  if (run) {
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

void Task::on_cancelled(DispatcherPtr d, CallbackPtr cb) {
  Work work(std::move(d), std::move(cb));
  auto lock = base::acquire_lock(mu_);
  RC rc;
  bool run;
  switch (state_) {
    case State::ready:
    case State::running:
      run = false;
      on_cancel_.push_back(std::move(work));
      break;

    case State::expiring:
    case State::cancelling:
      run = true;
      break;

    case State::unstarted:
    case State::done:
      rc = result_.code();
      run = (rc == RC::DEADLINE_EXCEEDED || rc == RC::CANCELLED);
  }
  lock.unlock();
  if (run) invoke(std::move(work));
}

void Task::on_finished(DispatcherPtr d, CallbackPtr cb) {
  Work work(std::move(d), std::move(cb));
  auto lock = base::acquire_lock(mu_);
  switch (state_) {
    case State::ready:
    case State::running:
    case State::expiring:
    case State::cancelling:
    case State::unstarted:
      on_finish_.push_back(std::move(work));
      break;

    case State::done:
      lock.unlock();
      invoke(std::move(work));
  }
}

void Task::expire() noexcept {
  return cancel_impl(State::expiring, base::Result::deadline_exceeded());
}

void Task::cancel() noexcept {
  return cancel_impl(State::cancelling, base::Result::cancelled());
}

void Task::cancel_impl(State next, base::Result result) noexcept {
  auto lock = base::acquire_lock(mu_);
  switch (state_) {
    case State::ready:
      state_ = State::unstarted;
      result_ = std::move(result);
      break;

    case State::running:
    case State::expiring:
    case State::cancelling:
      if (state_ >= next) return;
      state_ = next;
      break;

    case State::unstarted:
    case State::done:
      return;
  }
  auto on_cancel = std::move(on_cancel_);
  auto subtasks = std::move(subtasks_);
  lock.unlock();

  lifo_callbacks(std::move(on_cancel));
  lifo_subtasks(std::move(subtasks));
}

bool Task::start() {
  auto lock = base::acquire_lock(mu_);
  switch (state_) {
    case State::ready:
      state_ = State::running;
      return true;

    case State::unstarted:
      finish_impl(lock);
      return false;

    default:
      LOG(DFATAL) << "BUG! event::Task: start() on started task";
      return false;
  }
}

void Task::finish(base::Result result) {
  auto lock = base::acquire_lock(mu_);
  assert_running(state_);
  result_ = std::move(result);
  finish_impl(lock);
}

void Task::finish_cancel() {
  auto lock = base::acquire_lock(mu_);
  assert_running(state_);
  auto r = base::Result::cancelled();
  if (state_ == State::expiring) r = base::Result::deadline_exceeded();
  result_ = std::move(r);
  finish_impl(lock);
}

void Task::finish_exception(std::exception_ptr eptr) {
  auto lock = base::acquire_lock(mu_);
  assert_running(state_);
  result_ = exception_result();
  eptr_ = eptr;
  finish_impl(lock);
}

void Task::finish_impl(base::Lock& lock) {
  state_ = State::done;
  auto on_finish = std::move(on_finish_);
  auto on_cancel = std::move(on_cancel_);
  auto subtasks = std::move(subtasks_);
  lock.unlock();

  RC rc = result_.code();
  if (rc == RC::DEADLINE_EXCEEDED || rc == RC::CANCELLED) {
    lifo_callbacks(std::move(on_cancel));
  }
  lifo_subtasks(std::move(subtasks));
  lifo_callbacks(std::move(on_finish));
}

}  // namespace event
