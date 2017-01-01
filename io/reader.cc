// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/reader.h"

#ifndef HAVE_SPLICE
#define HAVE_SPLICE 1
#endif

#ifndef HAVE_SENDFILE
#define HAVE_SENDFILE 1
#endif

#if HAVE_SENDFILE
#include <sys/sendfile.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <condition_variable>
#include <cstring>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>

#include "base/cleanup.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "io/writer.h"

using RC = base::Result::Code;

static constexpr std::size_t kDefaultIdealBlockSize = 1U << 20;  // 1 MiB
static constexpr std::size_t kSendfileMax = 4U << 20;            // 4 MiB
static constexpr std::size_t kSpliceMax = 4U << 20;              // 4 MiB

static base::Result reader_closed() {
  return base::Result::failed_precondition("io::Reader is closed");
}

static void propagate_result(event::Task* dst, const event::Task* src) {
  try {
    dst->finish(src->result());
  } catch (...) {
    dst->finish_exception(std::current_exception());
  }
}

static bool propagate_failure(event::Task* dst, const event::Task* src) {
  try {
    base::Result r = src->result();
    if (r) return true;
    dst->finish(std::move(r));
  } catch (...) {
    dst->finish_exception(std::current_exception());
  }
  return false;
}

namespace io {

static TransferMode default_transfer_mode() noexcept {
  return io::TransferMode::read_write;  // TODO: probe this on first access
}

bool ReaderImpl::prologue(event::Task* task, char* out, std::size_t* n,
                          std::size_t min, std::size_t max) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(n);
  CHECK_LE(min, max);
  *n = 0;
  return task->start();
}

bool ReaderImpl::prologue(event::Task* task, std::size_t* n, std::size_t max,
                          const Writer& w) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(n);
  w.assert_valid();
  *n = 0;
  return task->start();
}

bool ReaderImpl::prologue(event::Task* task) {
  CHECK_NOTNULL(task);
  return task->start();
}

void ReaderImpl::write_to(event::Task* task, std::size_t* n, std::size_t max,
                          const Writer& w, const Options& opts) {
  if (prologue(task, n, max, w)) task->finish(base::Result::not_implemented());
}

void Reader::assert_valid() const {
  if (!ptr_) {
    LOG(FATAL) << "BUG: io::Reader is empty!";
  }
}

namespace {
struct StringReadHelper {
  event::Task* const task;
  std::string* const out;
  event::Task subtask;
  BufferPool pool;
  OwnedBuffer buffer;
  std::size_t n;
  bool give_back;

  StringReadHelper(event::Task* t, std::string* o, BufferPool p, OwnedBuffer b,
                   bool g)
      : task(t),
        out(o),
        pool(std::move(p)),
        buffer(std::move(b)),
        n(0),
        give_back(g) {}

  base::Result run() {
    out->append(buffer.data(), n);
    if (give_back) pool.give(std::move(buffer));
    propagate_result(task, &subtask);
    delete this;
    return base::Result();
  }
};
}  // anonymous namespace

void Reader::read(event::Task* task, std::string* out, std::size_t min,
                  std::size_t max, const Options& opts) const {
  out->clear();
  if (!task->start()) return;

  BufferPool pool = opts.pool();
  OwnedBuffer buf;
  bool give_back;
  if (pool.pool_size() >= max) {
    buf = pool.take();
    give_back = true;
  } else {
    buf = OwnedBuffer(max);
    give_back = false;
  }

  auto* helper = new StringReadHelper(task, out, std::move(pool),
                                      std::move(buf), give_back);
  task->add_subtask(&helper->subtask);
  read(&helper->subtask, helper->buffer.data(), &helper->n, min, max, opts);
  auto closure = [helper] { return helper->run(); };
  helper->subtask.on_finished(event::callback(closure));
}

base::Result Reader::read(char* out, std::size_t* n, std::size_t min,
                          std::size_t max, const Options& opts) const {
  event::Task task;
  read(&task, out, n, min, max, opts);
  event::wait(opts.manager(), &task);
  return task.result();
}

base::Result Reader::read(std::string* out, std::size_t min, std::size_t max,
                          const Options& opts) const {
  event::Task task;
  read(&task, out, min, max, opts);
  event::wait(opts.manager(), &task);
  return task.result();
}

base::Result Reader::write_to(std::size_t* n, std::size_t max, const Writer& w,
                              const Options& opts) const {
  event::Task task;
  write_to(&task, n, max, w, opts);
  event::wait(opts.manager(), &task);
  return task.result();
}

base::Result Reader::close(const Options& opts) const {
  event::Task task;
  close(&task, opts);
  event::wait(opts.manager(), &task);
  return task.result();
}

namespace {
class FunctionReader : public ReaderImpl {
 public:
  FunctionReader(ReadFn rfn, CloseFn cfn)
      : rfn_(std::move(rfn)), cfn_(std::move(cfn)) {}

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const Options& opts) override {
    rfn_(task, out, n, min, max, opts);
  }

  void close(event::Task* task, const Options& opts) override {
    cfn_(task, opts);
  }

 private:
  ReadFn rfn_;
  CloseFn cfn_;
};

class SyncFunctionReader : public ReaderImpl {
 public:
  SyncFunctionReader(SyncReadFn rfn, SyncCloseFn cfn)
      : rfn_(std::move(rfn)), cfn_(std::move(cfn)) {}

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const Options& opts) override {
    if (prologue(task, out, n, min, max))
      task->finish(rfn_(out, n, min, max, opts));
  }

  void close(event::Task* task, const Options& opts) override {
    if (prologue(task)) task->finish(cfn_(opts));
  }

 private:
  SyncReadFn rfn_;
  SyncCloseFn cfn_;
};

class CloseIgnoringReader : public ReaderImpl {
 public:
  CloseIgnoringReader(Reader r) : r_(std::move(r)) {}

  std::size_t ideal_block_size() const noexcept override {
    return r_.ideal_block_size();
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const Options& opts) override {
    r_.read(task, out, n, min, max, opts);
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w, const Options& opts) override {
    r_.write_to(task, n, max, w, opts);
  }

  void close(event::Task* task, const Options& opts) override {
    if (prologue(task)) task->finish_ok();
  }

 private:
  Reader r_;
};

class LimitedReader : public ReaderImpl {
 public:
  LimitedReader(Reader r, std::size_t max)
      : r_(std::move(r)), remaining_(max) {}

  std::size_t ideal_block_size() const noexcept override {
    return r_.ideal_block_size();
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const Options& opts) override {
    if (!prologue(task, out, n, min, max)) return;

    auto lock = base::acquire_lock(mu_);
    std::size_t amax = std::min(max, remaining_);
    std::size_t amin = std::min(min, remaining_);
    bool eof = (amax < min);

    auto* subtask = new event::Task;
    task->add_subtask(subtask);
    r_.read(subtask, out, n, amin, amax, opts);

    auto closure = [this, task, n, subtask, eof](base::Lock lock) {
      remaining_ -= *n;
      lock.unlock();
      if (propagate_failure(task, subtask)) {
        if (eof)
          task->finish(base::Result::eof());
        else
          task->finish_ok();
      }
      delete subtask;
      return base::Result();
    };
    subtask->on_finished(event::callback(closure, std::move(lock)));
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w, const Options& opts) override {
    *n = 0;

    auto* lock = new base::Lock(mu_);
    if (max > remaining_) max = remaining_;
    r_.write_to(task, n, max, w, opts);

    auto closure = [this, n, lock] {
      remaining_ -= *n;
      delete lock;
      return base::Result();
    };
    task->on_finished(event::callback(closure));
  }

  void close(event::Task* task, const Options& opts) override {
    r_.close(task, opts);
  }

 private:
  Reader r_;
  mutable std::mutex mu_;
  std::size_t remaining_;
};

class StringOrBufferReader : public ReaderImpl {
 public:
  StringOrBufferReader(ConstBuffer buf) noexcept : buf_(buf),
                                                   pos_(0),
                                                   closed_(false) {}
  StringOrBufferReader(std::string str) noexcept : str_(std::move(str)),
                                                   buf_(ConstBuffer(str_)),
                                                   pos_(0),
                                                   closed_(false) {}

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const Options& opts) override {
    if (!prologue(task, out, n, min, max)) return;

    auto lock = base::acquire_lock(mu_);

    if (closed_) {
      task->finish(reader_closed());
      return;
    }

    std::size_t len = buf_.size() - pos_;
    if (len > max) len = max;
    ::memcpy(out, buf_.data() + pos_, len);
    pos_ += len;

    lock.unlock();

    *n = len;
    if (min > len)
      task->finish(base::Result::eof());
    else
      task->finish_ok();
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w, const Options& opts) override {
    *n = 0;

    auto* lock = new base::Lock(mu_);

    if (closed_) {
      delete lock;
      if (task->start()) task->finish(reader_closed());
      return;
    }

    const char* ptr = buf_.data() + pos_;
    std::size_t len = buf_.size() - pos_;
    if (len > max) len = max;

    w.write(task, n, ptr, len, opts);

    auto closure = [this, n, lock] {
      pos_ += *n;
      delete lock;
      return base::Result();
    };
    task->on_finished(event::callback(closure));
  }

  void close(event::Task* task, const Options& opts) override {
    auto lock = base::acquire_lock(mu_);
    bool was = closed_;
    closed_ = true;
    lock.unlock();
    if (prologue(task)) {
      if (was)
        task->finish(reader_closed());
      else
        task->finish_ok();
    }
  }

 private:
  const std::string str_;
  const ConstBuffer buf_;
  mutable std::mutex mu_;
  std::size_t pos_;
  bool closed_;
};

class NullReader : public ReaderImpl {
 public:
  NullReader() noexcept = default;

  std::size_t ideal_block_size() const noexcept override { return 64; }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const Options& opts) override {
    if (!prologue(task, out, n, min, max)) return;
    base::Result r;
    if (min > 0) r = base::Result::eof();
    *n = 0;
    task->finish(std::move(r));
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w, const Options& opts) override {
    if (!prologue(task, n, max, w)) return;
    *n = 0;
    task->finish_ok();
  }

  void close(event::Task* task, const Options& opts) override {
    if (prologue(task)) task->finish_ok();
  }
};

class ZeroReader : public ReaderImpl {
 public:
  ZeroReader() noexcept = default;

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const Options& opts) override {
    if (!prologue(task, out, n, min, max)) return;
    if (max > 0) ::bzero(out, max);
    *n = max;
    task->finish_ok();
  }

  void close(event::Task* task, const Options& opts) override {
    if (prologue(task)) task->finish_ok();
  }
};

class FDReader : public ReaderImpl {
 public:
  struct Op {
    virtual ~Op() noexcept = default;
    virtual void cancel() = 0;
    virtual bool process(FDReader* reader) = 0;
  };

  struct ReadOp : public Op {
    event::Task* const task;
    char* const out;
    std::size_t* const n;
    const std::size_t min;
    const std::size_t max;
    const Options options;
    event::FileDescriptor rdevt;

    ReadOp(event::Task* t, char* o, std::size_t* n, std::size_t mn,
           std::size_t mx, Options opts) noexcept : task(t),
                                                    out(o),
                                                    n(n),
                                                    min(mn),
                                                    max(mx),
                                                    options(std::move(opts)) {}
    void cancel() override { task->cancel(); }
    bool process(FDReader* reader) override;
  };

  struct WriteToOp : public Op {
    event::Task* const task;
    std::size_t* const n;
    const std::size_t max;
    const Writer writer;
    const Options options;
    event::FileDescriptor rdevt;
    event::FileDescriptor wrevt;

    WriteToOp(event::Task* t, std::size_t* n, std::size_t mx, Writer w,
              Options opts) noexcept : task(t),
                                       n(n),
                                       max(mx),
                                       writer(std::move(w)),
                                       options(std::move(opts)) {}
    void cancel() override { task->cancel(); }
    bool process(FDReader* reader) override;
  };

  explicit FDReader(base::FD fd) noexcept : fd_(std::move(fd)), depth_(0) {}
  ~FDReader() noexcept override;

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const Options& opts) override;
  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w, const Options& opts) override;
  void close(event::Task* task, const Options& opts) override;

  base::FD internal_readerfd() const override { return fd_; }

 private:
  void process(base::Lock& lock);
  base::Result wake(event::Set set);
  base::Result arm(event::FileDescriptor* evt, const base::FD& fd,
                   event::Set set, const Options& o);

  const base::FD fd_;
  mutable std::mutex mu_;
  std::condition_variable cv_;                // protected by mu_
  std::deque<std::unique_ptr<Op>> q_;         // protected by mu_
  std::vector<event::FileDescriptor> purge_;  // protected by mu_
  std::size_t depth_;                         // protected by mu_
};

FDReader::~FDReader() noexcept {
  VLOG(6) << "io::FDReader::~FDReader";
  auto lock = base::acquire_lock(mu_);
  while (depth_ != 0) cv_.wait(lock);
  auto q = std::move(q_);
  lock.unlock();
  for (auto& op : q) {
    op->cancel();
    op->process(this);
  }
  lock.lock();
  auto p = std::move(purge_);
  for (auto& evt : p) evt.wait();
}

void FDReader::read(event::Task* task, char* out, std::size_t* n,
                    std::size_t min, std::size_t max, const Options& opts) {
  if (!prologue(task, out, n, min, max)) return;
  auto lock = base::acquire_lock(mu_);
  VLOG(6) << "io::FDReader::read: min=" << min << ", max=" << max;
  q_.emplace_back(new ReadOp(task, out, n, min, max, opts));
  process(lock);
}

void FDReader::write_to(event::Task* task, std::size_t* n, std::size_t max,
                        const Writer& w, const Options& opts) {
  if (!prologue(task, n, max, w)) return;
  auto lock = base::acquire_lock(mu_);
  VLOG(6) << "io::FDReader::write_to: max=" << max;
  q_.emplace_back(new WriteToOp(task, n, max, w, opts));
  process(lock);
}

void FDReader::close(event::Task* task, const Options& opts) {
  VLOG(6) << "io::FDReader::close";
  base::Result r = fd_->close();
  if (prologue(task)) task->finish(std::move(r));
}

void FDReader::process(base::Lock& lock) {
  VLOG(4) << "io::FDReader::process: begin: q.size()=" << q_.size();

  while (!q_.empty()) {
    auto op = std::move(q_.front());
    q_.pop_front();
    lock.unlock();
    auto reacquire = base::cleanup([&lock] { lock.lock(); });
    bool completed = op->process(this);
    reacquire.run();
    if (!completed) {
      q_.push_front(std::move(op));
      break;
    }
    VLOG(5) << "io::FDReader::process: consumed";
  }

  if (event::internal::is_shallow()) {
    auto p = std::move(purge_);
    for (auto& evt : p) evt.wait();
  }

  VLOG(4) << "io::FDReader::process: end";
}

base::Result FDReader::wake(event::Set set) {
  VLOG(6) << "woke io::FDReader, set=" << set;
  auto lock = base::acquire_lock(mu_);
  ++depth_;
  auto cleanup = base::cleanup([this] {
    --depth_;
    if (depth_ == 0) cv_.notify_all();
  });
  process(lock);
  return base::Result();
}

base::Result FDReader::arm(event::FileDescriptor* evt, const base::FD& fd,
                           event::Set set, const Options& o) {
  DCHECK_NOTNULL(evt);
  base::Result r;
  if (!*evt) {
    event::Manager manager = o.manager();
    auto closure = [this](event::Data data) { return wake(data.events); };
    r = manager.fd(evt, fd, set, event::handler(closure));
  }
  return r;
}

bool FDReader::ReadOp::process(FDReader* reader) {
  VLOG(4) << "io::FDReader::ReadOp: begin: "
          << "*n=" << *n << ", "
          << "min=" << min << ", "
          << "max=" << max;

  // Disable any event and make sure someone waits on it.
  auto cleanup = base::cleanup([this, reader] {
    if (rdevt) {
      rdevt.disable().expect_ok(__FILE__, __LINE__);
      auto lock = base::acquire_lock(reader->mu_);
      reader->purge_.push_back(std::move(rdevt));
    }
  });

  // Check for cancellation
  if (!task->is_running()) {
    VLOG(4) << "io::FDReader::ReadOp: cancel";
    task->finish_cancel();
    return true;
  }

  const auto& rfd = reader->fd_;

  base::Result r;
  // Until we finish the read operation...
  while (*n < max) {
    // Attempt to read some data
    auto pair = rfd->acquire_fd();
    VLOG(5) << "io::FDReader::ReadOp: read: "
            << "fd=" << pair.first << ", "
            << "len=" << (max - *n);
    ssize_t len = ::read(pair.first, out + *n, max - *n);
    int err_no = errno;
    VLOG(6) << "io::FDReader::ReadOp: result=" << len;
    pair.second.unlock();

    // Check the return code
    if (len < 0) {
      // Interrupted by signal? Retry immediately
      if (err_no == EINTR) {
        VLOG(6) << "io::FDReader::ReadOp: EINTR";
        continue;
      }

      // No data for non-blocking read?
      // |min == 0| is success, otherwise reschedule for later
      if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
        VLOG(6) << "io::FDReader::ReadOp: EAGAIN";

        // If we've hit the minimum threshold, call it a day.
        if (*n >= min) break;

        // Register a callback for poll, if we didn't already.
        r = reader->arm(&rdevt, rfd, event::Set::readable_bit(), options);
        if (!r) break;

        cleanup.cancel();
        return false;
      }

      // Other error? Bomb out
      r = base::Result::from_errno(err_no, "read(2)");
      break;
    }
    if (len == 0) {
      VLOG(6) << "io::FDReader::ReadOp: EOF";
      if (*n < min) r = base::Result::eof();
      break;
    }
    *n += len;
  }
  VLOG(4) << "io::FDReader::ReadOp: end: "
          << "*n=" << *n << ", "
          << "r=" << r;
  task->finish(std::move(r));
  return true;
}

bool FDReader::WriteToOp::process(FDReader* reader) {
  VLOG(4) << "io::FDReader::WriteToOp: begin: "
          << "*n=" << *n << ", "
          << "max=" << max;

  // Disable any events and make sure someone waits on them.
  auto cleanup = base::cleanup([this, reader] {
    if (rdevt || wrevt) {
      rdevt.disable().expect_ok(__FILE__, __LINE__);
      wrevt.disable().expect_ok(__FILE__, __LINE__);
      auto lock = base::acquire_lock(reader->mu_);
      reader->purge_.push_back(std::move(rdevt));
      reader->purge_.push_back(std::move(wrevt));
    }
  });

  // Check for cancellation
  if (!task->is_running()) {
    VLOG(4) << "io::FDReader::WriteToOp: cancel";
    task->finish_cancel();
    return true;
  }

  auto xm = options.transfer_mode();
  if (xm == TransferMode::system_default) xm = default_transfer_mode();

  base::FD rfd = reader->fd_;
  base::FD wfd = writer.implementation()->internal_writerfd();
  base::Result r;

  // Try using splice(2)
  if (xm >= TransferMode::splice && wfd) {
    while (*n < max) {
      std::size_t cmax = max - *n;
      if (cmax > kSpliceMax) cmax = kSpliceMax;

      auto pair0 = wfd->acquire_fd();
      auto pair1 = rfd->acquire_fd();
      VLOG(5) << "io::FDReader::WriteToOp: splice: "
              << "wfd=" << pair0.first << ", "
              << "rfd=" << pair1.first << ", "
              << "max=" << cmax << ", "
              << "*n=" << *n;
      ssize_t sent = ::splice(pair1.first, nullptr, pair0.first, nullptr, cmax,
                              SPLICE_F_NONBLOCK);
      int err_no = errno;
      VLOG(6) << "io::FDReader::WriteToOp: result=" << sent;
      pair1.second.unlock();
      pair0.second.unlock();

      // Check the return code
      if (sent < 0) {
        // Interrupted by signal? Retry immediately
        if (err_no == EINTR) {
          VLOG(6) << "io::FDReader::WriteToOp: EINTR";
          continue;
        }

        // splice(2) not implemented, at all?
        if (err_no == ENOSYS) {
          VLOG(6) << "io::FDReader::WriteToOp: ENOSYS";
          goto no_splice;
        }
        // splice(2) not implemented, for this pair of fds?
        if (err_no == EINVAL) {
          VLOG(6) << "io::FDReader::WriteToOp: EINVAL";
          goto no_splice;
        }

        // No data for read, or full buffers for write?
        // Reschedule for later
        if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
          VLOG(6) << "io::FDReader::WriteToOp: EAGAIN";

          // Errno doesn't distinguish "reader is empty" from "writer is
          // full", so schedule on both of them.
          r = reader->arm(&rdevt, rfd, event::Set::readable_bit(), options);
          if (!r) break;
          r = reader->arm(&wrevt, wfd, event::Set::writable_bit(), options);
          if (!r) break;

          cleanup.cancel();
          return false;
        }

        // Other error? Bomb out
        r = base::Result::from_errno(err_no, "sendfile(2)");
        break;
      }
      if (sent == 0) {
        VLOG(6) << "io::FDReader::WriteToOp: EOF";
        break;
      }
      *n += sent;
    }
    goto finish;
  }
no_splice:

  // Try using sendfile(2)
  if (xm >= TransferMode::sendfile && wfd) {
    while (*n < max) {
      std::size_t cmax = max - *n;
      if (cmax > kSendfileMax) cmax = kSendfileMax;

      auto pair0 = wfd->acquire_fd();
      auto pair1 = rfd->acquire_fd();
      VLOG(5) << "io::FDReader::WriteToOp: sendfile: "
              << "wfd=" << pair0.first << ", "
              << "rfd=" << pair1.first << ", "
              << "max=" << cmax << ", "
              << "*n=" << *n;
      ssize_t sent = ::sendfile(pair0.first, pair1.first, nullptr, cmax);
      int err_no = errno;
      VLOG(6) << "io::FDReader::WriteToOp: result=" << sent;
      pair1.second.unlock();
      pair0.second.unlock();

      // Check the return code
      if (sent < 0) {
        // Interrupted by signal? Retry immediately
        if (err_no == EINTR) {
          VLOG(6) << "io::FDReader::WriteToOp: EINTR";
          continue;
        }

        // sendfile(2) not implemented, at all?
        if (err_no == ENOSYS) {
          VLOG(6) << "io::FDReader::WriteToOp: ENOSYS";
          goto no_sendfile;
        }
        // sendfile(2) not implemented, for this pair of fds?
        if (err_no == EINVAL) {
          VLOG(6) << "io::FDReader::WriteToOp: EINVAL";
          goto no_sendfile;
        }

        // No data for read, or full buffers for write?
        // Reschedule for later
        if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
          VLOG(6) << "io::FDReader::WriteToOp: EAGAIN";

          // Errno doesn't distinguish "reader is empty" from "writer is
          // full", so schedule on both of them.
          r = reader->arm(&rdevt, rfd, event::Set::readable_bit(), options);
          if (!r) break;
          r = reader->arm(&wrevt, wfd, event::Set::writable_bit(), options);
          if (!r) break;

          cleanup.cancel();
          return false;
        }

        // Other error? Bomb out
        r = base::Result::from_errno(err_no, "sendfile(2)");
        break;
      }
      if (sent == 0) {
        VLOG(6) << "io::FDReader::WriteToOp: EOF";
        break;
      }
      *n += sent;
    }
    goto finish;
  }
no_sendfile:

  // Nothing else left to try
  r = base::Result::not_implemented();

finish:
  VLOG(4) << "io::FDReader::WriteToOp: end: "
          << "*n=" << *n << ", "
          << "r=" << r;
  task->finish(std::move(r));
  return true;
}

class MultiReader : public ReaderImpl {
 public:
  struct Op {
    event::Task* const task;
    char* const out;
    std::size_t* const n;
    const std::size_t min;
    const std::size_t max;
    const Options options;
    event::Task subtask;
    std::size_t subn;

    Op(event::Task* t, char* o, std::size_t* n, std::size_t mn, std::size_t mx,
       Options opts) noexcept : task(t),
                                out(o),
                                n(n),
                                min(mn),
                                max(mx),
                                options(std::move(opts)),
                                subn(0) {}
    bool process(MultiReader* reader);
  };

  struct CloseHelper {
    event::Task* const task;
    const std::size_t size;
    const std::unique_ptr<event::Task[]> subtasks;
    mutable std::mutex mu;
    std::size_t pending;

    CloseHelper(event::Task* t, std::size_t sz) noexcept
        : task(t),
          size(sz),
          subtasks(new event::Task[size]),
          pending(size) {
      for (std::size_t i = 0; i < size; ++i) {
        task->add_subtask(&subtasks[i]);
      }
    }

    base::Result run() {
      auto lock = base::acquire_lock(mu);
      --pending;
      if (pending > 0) return base::Result();
      lock.unlock();
      bool propagated = false;
      for (std::size_t i = 0; i < size; ++i) {
        if (!propagate_failure(task, &subtasks[i])) {
          propagated = true;
          break;
        }
      }
      if (!propagated) task->finish_ok();
      delete this;
      return base::Result();
    }
  };

  explicit MultiReader(std::vector<Reader> vec) noexcept : vec_(std::move(vec)),
                                                           pass_(0),
                                                           curr_(0) {}

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;  // TODO: calculate LCM(vec[0], vec[1], ...)
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const Options& opts) override {
    if (!prologue(task, out, n, min, max)) return;
    auto lock = base::acquire_lock(mu_);
    q_.emplace_back(new Op(task, out, n, min, max, opts));
    VLOG(6) << "io::MultiReader::read";
    process(lock);
  }

  void close(event::Task* task, const Options& opts) override {
    std::size_t size = vec_.size();
    auto* helper = new CloseHelper(task, size);
    auto closure = [helper] { return helper->run(); };
    event::Task* subtasks = helper->subtasks.get();
    for (std::size_t i = 0; i < size; ++i) {
      event::Task* st = subtasks + i;
      task->add_subtask(st);
      vec_[i].close(st, opts);
      st->on_finished(event::callback(closure));
    }
  }

  void process(base::Lock& lock) {
    ++pass_;
    if (pass_ > 1) return;
    auto cleanup0 = base::cleanup([this] { --pass_; });

    VLOG(4) << "io::MultiReader::process: q.size()=" << q_.size();
    while (!q_.empty()) {
      std::unique_ptr<Op> op = std::move(q_.front());
      q_.pop_front();

      bool completed = false;
      while (pass_ > 0) {
        lock.unlock();
        auto cleanup1 = base::cleanup([&lock] { lock.lock(); });
        completed = op->process(this);
        cleanup1.run();
        if (completed) break;
        --pass_;
      }
      pass_ = 1;
      if (!completed) {
        q_.push_front(std::move(op));
        break;
      }

      VLOG(5) << "io::MultiReader::process: consumed";
    }
  }

 private:
  const std::vector<Reader> vec_;
  mutable std::mutex mu_;
  std::deque<std::unique_ptr<Op>> q_;  // protected by mu_
  std::size_t pass_;                   // protected by mu_
  std::size_t curr_;                   // protected by pass_
};

bool MultiReader::Op::process(MultiReader* reader) {
  *n += subn;

  VLOG(6) << "io::MultiReader::Op::process: *n=" << *n << ", subn=" << subn;

  RC code;
  if (subtask.is_finished()) {
    try {
      code = subtask.result().code();
    } catch (...) {
      code = RC::UNKNOWN;
    }
  } else {
    code = RC::OK;
  }

  if (code != RC::OK && code != RC::END_OF_FILE) {
    propagate_result(task, &subtask);
    return true;
  }

  if (*n >= min) {
    task->finish_ok();
    return true;
  }

  if (code == RC::END_OF_FILE) {
    ++reader->curr_;
  }
  if (reader->curr_ >= reader->vec_.size()) {
    task->finish(base::Result::eof());
    return true;
  }

  char* subout = out + *n;
  std::size_t submin = min - *n;
  std::size_t submax = max - *n;
  if (submin == 0 && submax > 0) submin = 1;

  auto& r = reader->vec_[reader->curr_];
  subtask.reset();
  task->add_subtask(&subtask);
  r.read(&subtask, subout, &subn, submin, submax, options);

  auto closure = [reader] {
    auto lock = base::acquire_lock(reader->mu_);
    reader->process(lock);
    return base::Result();
  };
  subtask.on_finished(event::callback(closure));
  return false;
}
}  // anonymous namespace

Reader reader(ReadFn rfn, CloseFn cfn) {
  return Reader(
      std::make_shared<FunctionReader>(std::move(rfn), std::move(cfn)));
}

Reader reader(SyncReadFn rfn, SyncCloseFn cfn) {
  return Reader(
      std::make_shared<SyncFunctionReader>(std::move(rfn), std::move(cfn)));
}

Reader ignore_close(Reader r) {
  return Reader(std::make_shared<CloseIgnoringReader>(std::move(r)));
}

Reader limited_reader(Reader r, std::size_t max) {
  return Reader(std::make_shared<LimitedReader>(std::move(r), max));
}

Reader stringreader(std::string str) {
  return Reader(std::make_shared<StringOrBufferReader>(std::move(str)));
}

Reader bufferreader(ConstBuffer buf) {
  return Reader(std::make_shared<StringOrBufferReader>(buf));
}

Reader nullreader() { return Reader(std::make_shared<NullReader>()); }

Reader zeroreader() { return Reader(std::make_shared<ZeroReader>()); }

Reader fdreader(base::FD fd) {
  return Reader(std::make_shared<FDReader>(std::move(fd)));
}

Reader multireader(std::vector<Reader> readers) {
  return Reader(std::make_shared<MultiReader>(std::move(readers)));
}

}  // namespace io
