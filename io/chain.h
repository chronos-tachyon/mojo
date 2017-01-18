// io/chain.h - Chained buffers
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_CHAIN_H
#define IO_CHAIN_H

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "base/strings.h"
#include "io/buffer.h"
#include "io/common.h"
#include "io/options.h"

namespace io {

// Chain is an intermediate class representing a chain of OwnedBuffer objects.
// It's used as a byte queue by Pipes, buffered Readers, and buffered Writers.
class Chain {
 public:
  // Func is a callback that requests that the Chain's owner should call some
  // sequence of |fill()|, |drain()|, |fail_reads()|, |fail_writes()|, and/or
  // |flush()| to unblock forward progress.
  //
  // - When called in the |rdfn| role, the goal is to fulfill a |read()|
  //   operation, so the Chain's owner should call |fill()| or |fail_reads()|.
  //
  //   In the case of |fail_reads()|, |fail_writes()| and/or |flush()| may also
  //   be appropriate.
  //
  // - When called in the |wrfn| role, the goal is to fulfill a |write()|
  //   operation, so the Chain's owner should call |drain()| or |fail_writes()|.
  //
  //   In the case of |fail_writes()|, |fail_reads()| may also be appropriate.
  //
  // Returns true if more processing should be attempted immediately, or false
  // if the Chain's owner will arrange for |process()| to be called at a later
  // time.
  //
  using Func = std::function<bool(Chain* chain, const base::Options& opts)>;

  Chain(Func rdfn, Func wrfn, PoolPtr pool, std::size_t max_buffers);

  const PoolPtr& pool() const noexcept { return pool_; }

  // Fill the queue with bytes.
  void fill(std::size_t* n, const char* ptr, std::size_t len);

  // Drain bytes from the queue.
  void drain(std::size_t* n, char* ptr, std::size_t len);

  // Once reads drain the queue, start returning an error on future reads.
  void fail_reads(base::Result r) noexcept;

  // Start returning an error on future writes.
  void fail_writes(base::Result r) noexcept;

  // Blow away the queue.  Only makes sense after |fail_reads()|.
  void flush() noexcept;

  // Process outstanding operations against the queue.  This MUST be called
  // after each sequence of |fill()|, |drain()|, |fail_reads()|,
  // |fail_writes()|, and/or |flush()| calls.
  void process() noexcept;

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts);

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts);

 private:
  enum class Progress : uint8_t {
    none = 0,
    partial = 1,
    complete = 2,
  };

  struct ReadOp {
    event::Task* task;
    char* out;
    std::size_t* n;
    std::size_t min;
    std::size_t max;
    base::Options options;

    ReadOp(event::Task* t, char* o, std::size_t* n, std::size_t mn,
           std::size_t mx, base::Options opts) noexcept
        : task(t),
          out(o),
          n(n),
          min(mn),
          max(mx),
          options(std::move(opts)) {}
  };

  struct WriteOp {
    event::Task* task;
    std::size_t* n;
    const char* ptr;
    std::size_t len;
    base::Options options;

    WriteOp(event::Task* t, std::size_t* n, const char* p, std::size_t l,
            base::Options opts) noexcept : task(t),
                                           n(n),
                                           ptr(p),
                                           len(l),
                                           options(std::move(opts)) {}
  };

  void xlate_locked(std::size_t* blocknum, std::size_t* offset,
                    std::size_t pos) const noexcept;
  void fill_locked(std::size_t* n, const char* ptr, std::size_t len) noexcept;
  void drain_locked(std::size_t* n, char* out, std::size_t len) noexcept;
  void process_locked(base::Lock& lock) noexcept;

  bool reads_locked(base::Lock& lock) noexcept;
  bool writes_locked(base::Lock& lock) noexcept;

  Progress read_locked(base::Lock& lock, const ReadOp* op) noexcept;
  Progress write_locked(base::Lock& lock, const WriteOp* op) noexcept;

  const Func rdfn_;
  const Func wrfn_;
  const PoolPtr pool_;
  const std::size_t max_;
  mutable std::mutex mu_;
  std::vector<OwnedBuffer> vec_;
  std::deque<std::unique_ptr<const ReadOp>> rdq_;
  std::deque<std::unique_ptr<const WriteOp>> wrq_;
  base::Result rderr_;
  base::Result wrerr_;
  std::size_t rdpos_;
  std::size_t wrpos_;
  bool rdbusy_;
  bool wrbusy_;
};

using ChainPtr = std::shared_ptr<Chain>;

ChainPtr make_chain(Chain::Func rdfn, Chain::Func wrfn, PoolPtr pool,
                    std::size_t max_buffers);
ChainPtr make_chain(Chain::Func rdfn, Chain::Func wrfn, PoolPtr pool);
ChainPtr make_chain(PoolPtr pool, std::size_t max_buffers);
ChainPtr make_chain(PoolPtr pool);

}  // namespace io

#endif  // IO_CHAIN_H
