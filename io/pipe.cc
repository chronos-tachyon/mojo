// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/pipe.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>

#include "base/cleanup.h"
#include "base/logging.h"
#include "io/buffer.h"
#include "io/chain.h"

namespace io {

namespace {

static base::Result closed_pipe() {
  return base::Result::failed_precondition("io::Pipe is closed");
}

static constexpr std::size_t kPipeIdealBlockSize = 1U << 16;  // 64 KiB
static constexpr std::size_t kPipeMaxBlocks = 16;

struct Guts {
  mutable std::mutex mu;
  Chain chain;
  bool rdclosed;
  bool wrclosed;

  Guts(PoolPtr pool, std::size_t max_buffers) noexcept
      : chain(nullptr, nullptr, std::move(pool), max_buffers),
        rdclosed(false),
        wrclosed(false) {}
};

using GutsPtr = std::shared_ptr<Guts>;

class PipeReader : public ReaderImpl {
 public:
  explicit PipeReader(GutsPtr guts) noexcept
      : guts_(DCHECK_NOTNULL(std::move(guts))),
        bufsz_(guts_->chain.pool()->buffer_size()) {}

  ~PipeReader() noexcept { close_impl(); }

  std::size_t ideal_block_size() const noexcept override { return bufsz_; }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override {
    guts_->chain.read(task, out, n, min, max, opts);
  }

  void close(event::Task* task, const base::Options& opts) override {
    bool was = close_impl();
    CHECK_NOTNULL(task);
    if (task->start()) {
      if (was)
        task->finish(closed_pipe());
      else
        task->finish_ok();
    }
  }

 private:
  bool close_impl() {
    auto lock = base::acquire_lock(guts_->mu);
    if (guts_->rdclosed) return true;
    auto r = closed_pipe();
    guts_->chain.fail_writes(r);
    guts_->chain.fail_reads(r);
    guts_->chain.flush();
    guts_->chain.process();
    guts_->rdclosed = true;
    guts_->wrclosed = true;
    return false;
  }

  GutsPtr guts_;
  const std::size_t bufsz_;
};

class PipeWriter : public WriterImpl {
 public:
  explicit PipeWriter(GutsPtr guts) noexcept
      : guts_(DCHECK_NOTNULL(std::move(guts))),
        bufsz_(guts_->chain.pool()->buffer_size()) {}

  ~PipeWriter() noexcept { close_impl(); }

  std::size_t ideal_block_size() const noexcept override { return bufsz_; }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
    guts_->chain.write(task, n, ptr, len, opts);
  }

  void close(event::Task* task, const base::Options& opts) override {
    bool was = close_impl();
    CHECK_NOTNULL(task);
    if (task->start()) {
      if (was)
        task->finish(closed_pipe());
      else
        task->finish_ok();
    }
  }

 private:
  bool close_impl() {
    auto lock = base::acquire_lock(guts_->mu);
    if (guts_->wrclosed) return true;
    guts_->chain.fail_writes(closed_pipe());
    guts_->chain.fail_reads(base::Result::eof());
    guts_->chain.process();
    guts_->wrclosed = true;
    return false;
  }

  GutsPtr guts_;
  const std::size_t bufsz_;
};

}  // anonymous namespace

Pipe make_pipe() { return make_pipe(kPipeIdealBlockSize, kPipeMaxBlocks); }

Pipe make_pipe(std::size_t block_size, std::size_t max_buffers) {
  auto pool = make_pool(block_size, max_buffers);
  return make_pipe(std::move(pool), max_buffers);
}

Pipe make_pipe(PoolPtr pool) {
  CHECK_NOTNULL(pool.get());
  auto max = std::max(std::size_t(3), pool->max());
  return make_pipe(std::move(pool), max);
}

Pipe make_pipe(PoolPtr pool, std::size_t max_buffers) {
  CHECK_NOTNULL(pool.get());
  CHECK_GE(max_buffers, 3U);
  auto guts = std::make_shared<Guts>(std::move(pool), max_buffers);
  return Pipe(Reader(std::make_shared<PipeReader>(guts)),
              Writer(std::make_shared<PipeWriter>(guts)));
}

void make_pipe(Reader* r, Writer* w) {
  CHECK_NOTNULL(r);
  CHECK_NOTNULL(w);
  auto pipe = make_pipe();
  *r = std::move(pipe.read);
  *w = std::move(pipe.write);
}

}  // namespace io
