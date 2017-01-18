// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/chain.h"

#include <cstring>

#include "base/backport.h"
#include "base/cleanup.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "io/reader.h"
#include "io/writer.h"

namespace io {

Chain::Chain(Func rdfn, Func wrfn, PoolPtr pool, std::size_t max_buffers)
    : rdfn_(std::move(rdfn)),
      wrfn_(std::move(wrfn)),
      pool_(CHECK_NOTNULL(std::move(pool))),
      max_(max_buffers),
      rdpos_(0),
      wrpos_(0),
      rdbusy_(false),
      wrbusy_(false) {
  CHECK_GT(pool_->buffer_size(), 0U);
  vec_.reserve(max_);
}

void Chain::fill(std::size_t* n, const char* ptr, std::size_t len) {
  CHECK_NOTNULL(n);
  if (len > 0) CHECK_NOTNULL(ptr);

  auto lock = base::acquire_lock(mu_);
  fill_locked(n, ptr, len);
}

void Chain::drain(std::size_t* n, char* out, std::size_t len) {
  CHECK_NOTNULL(n);
  if (len > 0) CHECK_NOTNULL(out);

  auto lock = base::acquire_lock(mu_);
  drain_locked(n, out, len);
}

void Chain::fail_reads(base::Result r) noexcept {
  CHECK(!r);
  auto lock = base::acquire_lock(mu_);
  rderr_ = std::move(r);
}

void Chain::fail_writes(base::Result r) noexcept {
  CHECK(!r);
  auto lock = base::acquire_lock(mu_);
  wrerr_ = std::move(r);
}

void Chain::flush() noexcept {
  auto lock = base::acquire_lock(mu_);
  while (!vec_.empty()) {
    pool_->give(std::move(vec_.back()));
    vec_.pop_back();
  }
  rdpos_ = wrpos_ = 0;
  DCHECK_LE(rdpos_, wrpos_);
}

void Chain::process() noexcept {
  auto lock = base::acquire_lock(mu_);
  process_locked(lock);
}

void Chain::read(event::Task* task, char* out, std::size_t* n, std::size_t min,
                 std::size_t max, const base::Options& opts) {
  if (!ReaderImpl::prologue(task, out, n, min, max)) return;
  auto lock = base::acquire_lock(mu_);
  auto op =
      base::backport::make_unique<const ReadOp>(task, out, n, min, max, opts);
  rdq_.push_back(std::move(op));
  process_locked(lock);
}

void Chain::write(event::Task* task, std::size_t* n, const char* ptr,
                  std::size_t len, const base::Options& opts) {
  if (!WriterImpl::prologue(task, n, ptr, len)) return;
  auto lock = base::acquire_lock(mu_);
  auto op = base::backport::make_unique<const WriteOp>(task, n, ptr, len, opts);
  wrq_.push_back(std::move(op));
  process_locked(lock);
}

void Chain::xlate_locked(std::size_t* blocknum, std::size_t* offset,
                         std::size_t pos) const noexcept {
  DCHECK_NOTNULL(blocknum);
  DCHECK_NOTNULL(offset);
  std::size_t x, y, z;
  z = pool_->buffer_size();
  DCHECK_GT(z, 0U);
  x = pos / z;
  y = pos - x * z;
  DCHECK_GE(x, 0U);
  DCHECK_LE(x, vec_.size());
  DCHECK(x < vec_.size() || y == 0);
  *blocknum = x;
  *offset = y;
}

void Chain::fill_locked(std::size_t* n, const char* ptr,
                        std::size_t len) noexcept {
  DCHECK_LE(rdpos_, wrpos_);
  std::size_t blocknum, offset;
  while (*n < len) {
    xlate_locked(&blocknum, &offset, wrpos_);
    while (blocknum >= vec_.size()) {
      if (vec_.size() >= max_) break;
      vec_.push_back(pool_->take());
    }
    auto& buf = vec_[blocknum];
    std::size_t sz = buf.size();
    DCHECK_EQ(sz, pool_->buffer_size());
    DCHECK_GT(sz, offset);
    std::size_t wrnum = std::min(len - *n, sz - offset);
    ::memcpy(buf.data() + offset, ptr + *n, wrnum);
    *n += wrnum;
    wrpos_ += wrnum;
    DCHECK_LE(rdpos_, wrpos_);
  }
  DCHECK_LE(*n, len);
}

void Chain::drain_locked(std::size_t* n, char* ptr, std::size_t len) noexcept {
  DCHECK_LE(rdpos_, wrpos_);
  std::size_t blocknum, offset;
  while (*n < len) {
    xlate_locked(&blocknum, &offset, rdpos_);
    if (blocknum >= vec_.size()) break;
    if (rdpos_ >= wrpos_) break;
    auto& buf = vec_[blocknum];
    std::size_t sz = buf.size();
    DCHECK_EQ(blocknum, 0U);
    DCHECK_EQ(sz, pool_->buffer_size());
    DCHECK_GT(sz, offset);
    std::size_t rdnum =
        std::min(len - *n, std::min(sz - offset, wrpos_ - rdpos_));
    ::memcpy(ptr + *n, buf.data() + offset, rdnum);
    *n += rdnum;
    rdpos_ += rdnum;
    DCHECK_LE(rdpos_, wrpos_);
    if (offset + rdnum == sz) {
      pool_->give(std::move(buf));
      vec_.erase(vec_.begin());
      rdpos_ -= sz;
      wrpos_ -= sz;
      DCHECK_LE(rdpos_, wrpos_);
    }
  }
  DCHECK_LE(*n, len);
}

void Chain::process_locked(base::Lock& lock) noexcept {
  bool write_progress = true;
  bool read_progress = true;
  while (write_progress || read_progress) {
    write_progress = writes_locked(lock);
    read_progress = reads_locked(lock);
  }
}

bool Chain::reads_locked(base::Lock& lock) noexcept {
  if (rdbusy_) return false;
  rdbusy_ = true;
  auto cleanup = base::cleanup([this] { rdbusy_ = false; });

  bool some = false;
  bool want = false;
  while (!rdq_.empty()) {
    auto op = std::move(rdq_.front());
    rdq_.pop_front();
    auto progress = read_locked(lock, op.get());
    if (progress != Progress::none) some = true;
    if (progress != Progress::complete) {
      want = true;
      rdq_.push_front(std::move(op));
      break;
    }
  }

  if (want && rdfn_) {
    const auto& opts = rdq_.front()->options;
    lock.unlock();
    auto reacquire = base::cleanup([&lock] { lock.lock(); });
    some = rdfn_(this, opts) || some;
  }

  return some;
}

bool Chain::writes_locked(base::Lock& lock) noexcept {
  if (wrbusy_) return false;
  wrbusy_ = true;
  auto cleanup = base::cleanup([this] { wrbusy_ = false; });

  bool some = false;
  bool want = false;
  while (!wrq_.empty()) {
    auto op = std::move(wrq_.front());
    wrq_.pop_front();
    auto progress = write_locked(lock, op.get());
    if (progress != Progress::none) some = true;
    if (progress != Progress::complete) {
      want = true;
      wrq_.push_front(std::move(op));
      break;
    }
  }

  if (want && wrfn_) {
    const auto& opts = wrq_.front()->options;
    lock.unlock();
    auto reacquire = base::cleanup([&lock] { lock.lock(); });
    some = wrfn_(this, opts) || some;
  }

  return some;
}

Chain::Progress Chain::read_locked(base::Lock& lock,
                                   const ReadOp* op) noexcept {
  auto oldn = *op->n;
  drain_locked(op->n, op->out, op->max);
  auto newn = *op->n;
  if (newn >= op->min) {
    op->task->finish_ok();
    return Chain::Progress::complete;
  }
  if (!rderr_) {
    op->task->finish(rderr_);
    return Chain::Progress::complete;
  }
  if (newn > oldn) {
    return Chain::Progress::partial;
  }
  return Chain::Progress::none;
}

Chain::Progress Chain::write_locked(base::Lock& lock,
                                    const WriteOp* op) noexcept {
  if (!wrerr_) {
    op->task->finish(wrerr_);
    return Chain::Progress::complete;
  }
  auto oldn = *op->n;
  fill_locked(op->n, op->ptr, op->len);
  auto newn = *op->n;
  if (newn >= op->len) {
    op->task->finish_ok();
    return Chain::Progress::complete;
  }
  if (newn > oldn) {
    return Chain::Progress::partial;
  }
  return Chain::Progress::none;
}

ChainPtr make_chain(Chain::Func rdfn, Chain::Func wrfn, PoolPtr pool,
                    std::size_t max_buffers) {
  return std::make_shared<Chain>(std::move(rdfn), std::move(wrfn),
                                 std::move(pool), max_buffers);
}

ChainPtr make_chain(Chain::Func rdfn, Chain::Func wrfn, PoolPtr pool) {
  std::size_t max_buffers = CHECK_NOTNULL(pool.get())->max();
  return std::make_shared<Chain>(std::move(rdfn), std::move(wrfn),
                                 std::move(pool), max_buffers);
}

ChainPtr make_chain(PoolPtr pool, std::size_t max_buffers) {
  return make_chain(nullptr, nullptr, std::move(pool), max_buffers);
}

ChainPtr make_chain(PoolPtr pool) {
  return make_chain(nullptr, nullptr, std::move(pool));
}

}  // namespace io
