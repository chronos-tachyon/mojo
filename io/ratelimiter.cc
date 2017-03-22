// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/ratelimiter.h"

#include <deque>
#include <mutex>

#include "base/backport.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/time/clock.h"
#include "base/time/time.h"

namespace io {

inline namespace implementation {
static std::size_t min(std::size_t a, std::size_t b) noexcept {
  return (a < b) ? a : b;
}

struct RateLimiterItem {
  event::Task* const task;
  const base::Options options;
  std::size_t remaining;
  event::Handle timer;

  RateLimiterItem(event::Task* t, std::size_t n, base::Options o) noexcept
      : task(t),
        options(o),
        remaining(n) {}
};

using Item = RateLimiterItem;
using Pointer = std::unique_ptr<Item>;

class BasicRateLimiter : public RateLimiterImpl {
 public:
  BasicRateLimiter(base::time::Duration ww, std::size_t wc,
                   std::size_t wb) noexcept : ww_(ww),
                                              wc_(wc),
                                              wb_(wb),
                                              bank_(wb) {}

  void gate(event::Task* task, std::size_t n,
            const base::Options& opts) override;

  void process();
  void process_impl(base::Lock& lock, base::time::MonotonicTime now);

 private:
  const base::time::Duration ww_;
  const std::size_t wc_;
  const std::size_t wb_;
  mutable std::mutex mu_;
  std::deque<Pointer> q_;
  base::time::MonotonicTime last_;
  std::size_t bank_;
};

void BasicRateLimiter::gate(event::Task* task, std::size_t n,
                            const base::Options& opts) {
  CHECK_NOTNULL(task);
  auto lock = base::acquire_lock(mu_);
  auto now = base::time::monotonic_now();
  q_.push_back(base::backport::make_unique<Item>(task, n, opts));
  process_impl(lock, now);
}

void BasicRateLimiter::process() {
  auto lock = base::acquire_lock(mu_);
  auto now = base::time::monotonic_now();
  process_impl(lock, now);
}

void BasicRateLimiter::process_impl(base::Lock& lock,
                                    base::time::MonotonicTime now) {
  if (!last_.is_epoch()) {
    base::time::Duration delta = now - last_;
    std::size_t earned = ::ceil(wc_ * (delta / ww_));
    bank_ = min(bank_ + earned, wb_);
  }
  last_ = now;

  while (!q_.empty()) {
    Item* item = q_.front().get();
    if (bank_ < item->remaining) break;
    bank_ -= item->remaining;
    item->task->finish_ok();
    q_.pop_front();
  }

  if (q_.empty()) return;
  Item* item = q_.front().get();
  item->remaining -= bank_;
  bank_ = 0;
  event::Manager m = get_manager(item->options);
  auto r = m.timer(&item->timer, event::handler([this](event::Data) {
    process();
    return base::Result();
  }));
  if (r) {
    auto end = now + ww_ * item->remaining / wc_;
    r = item->timer.set_at(end);
  }
  if (!r) {
    item->task->finish(std::move(r));
    q_.pop_front();
  }
}

class RateLimitedReader : public ReaderImpl {
 public:
  RateLimitedReader(Reader r, RateLimiter l) noexcept : r_(std::move(r)),
                                                        l_(std::move(l)) {}

  std::size_t ideal_block_size() const noexcept override {
    return r_.ideal_block_size();
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override;

  void close(event::Task* task, const base::Options& opts) override {
    r_.close(task, opts);
  }

 private:
  Reader r_;
  RateLimiter l_;
};

void RateLimitedReader::read(event::Task* task, char* out, std::size_t* n,
                             std::size_t min, std::size_t max,
                             const base::Options& opts) {
  struct Helper {
    event::Task subtask;
    RateLimitedReader* const self;
    event::Task* const task;
    std::size_t* const n;
    base::Options options;

    Helper(RateLimitedReader* s, event::Task* t, std::size_t* n,
           base::Options o) noexcept : self(s),
                                       task(t),
                                       n(n),
                                       options(std::move(o)) {}

    void read_complete() {
      if (event::propagate_failure(task, &subtask)) {
        delete this;
        return;
      }
      subtask.reset();
      task->add_subtask(&subtask);
      self->l_->gate(&subtask, *n, options);
      subtask.on_finished(event::callback([this] {
        gate_complete();
        return base::Result();
      }));
    }

    void gate_complete() {
      event::propagate_result(task, &subtask);
      delete this;
    }
  };

  if (!prologue(task, out, n, min, max)) return;
  auto* h = new Helper(this, task, n, opts);
  task->add_subtask(&h->subtask);
  r_.read(&h->subtask, out, n, min, max, opts);
  h->subtask.on_finished(event::callback([h] {
    h->read_complete();
    return base::Result();
  }));
}

class RateLimitedWriter : public WriterImpl {
 public:
  RateLimitedWriter(Writer w, RateLimiter l) noexcept : w_(std::move(w)),
                                                        l_(std::move(l)) {}

  std::size_t ideal_block_size() const noexcept override {
    return w_.ideal_block_size();
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override;

  void close(event::Task* task, const base::Options& opts) override {
    w_.close(task, opts);
  }

 private:
  Writer w_;
  RateLimiter l_;
};

void RateLimitedWriter::write(event::Task* task, std::size_t* n,
                              const char* ptr, std::size_t len,
                              const base::Options& opts) {
  struct Helper {
    event::Task subtask;
    RateLimitedWriter* const self;
    event::Task* const task;
    std::size_t* const n;
    base::Options options;

    Helper(RateLimitedWriter* s, event::Task* t, std::size_t* n,
           base::Options o) noexcept : self(s),
                                       task(t),
                                       n(n),
                                       options(std::move(o)) {}

    void write_complete() {
      if (event::propagate_failure(task, &subtask)) {
        delete this;
        return;
      }
      subtask.reset();
      task->add_subtask(&subtask);
      self->l_->gate(&subtask, *n, options);
      subtask.on_finished(event::callback([this] {
        gate_complete();
        return base::Result();
      }));
    }

    void gate_complete() {
      event::propagate_result(task, &subtask);
      delete this;
    }
  };

  if (!prologue(task, n, ptr, len)) return;
  auto* h = new Helper(this, task, n, opts);
  task->add_subtask(&h->subtask);
  w_.write(&h->subtask, n, ptr, len, opts);
  h->subtask.on_finished(event::callback([h] {
    h->write_complete();
    return base::Result();
  }));
}

}  // inline namespace implementation

base::Result RateLimiterImpl::gate(std::size_t n, const base::Options& opts) {
  event::Task task;
  gate(&task, n, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

RateLimiter new_ratelimiter(base::time::Duration window, std::size_t count,
                            std::size_t burst) {
  CHECK(!window.is_zero()) << ": " << window;
  CHECK(!window.is_neg()) << ": " << window;
  CHECK_GT(count, 0U);
  if (burst < count) burst = count;
  return std::make_shared<BasicRateLimiter>(window, count, burst);
}

Reader ratelimitedreader(Reader r, RateLimiter l) {
  return Reader(
      std::make_shared<RateLimitedReader>(std::move(r), std::move(l)));
}

Writer ratelimitedwriter(Writer w, RateLimiter l) {
  return Writer(
      std::make_shared<RateLimitedWriter>(std::move(w), std::move(l)));
}

}  // namespace io
