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

#include <cstring>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>

#include "base/cleanup.h"
#include "base/logging.h"
#include "base/util.h"
#include "io/writer.h"

using RC = base::Result::Code;

static constexpr std::size_t kSendfileMax = 4U << 20;  // 4 MiB
static constexpr std::size_t kSpliceMax = 4U << 20;    // 4 MiB

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

static TransferMode transfer_mode(const Options& ro, const Options& wo) {
  auto rxm = ro.transfer_mode();
  auto wxm = wo.transfer_mode();
  if (rxm == TransferMode::system_default) rxm = default_transfer_mode();
  if (wxm == TransferMode::system_default) wxm = default_transfer_mode();
  if (rxm < wxm)
    return rxm;
  else
    return wxm;
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
                          const Writer& w) {
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
                  std::size_t max) const {
  out->clear();
  if (!task->start()) return;

  BufferPool pool = options().pool();
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
  read(&helper->subtask, helper->buffer.data(), &helper->n, min, max);
  auto closure = [helper] { return helper->run(); };
  helper->subtask.on_finished(event::callback(closure));
}

base::Result Reader::read(char* out, std::size_t* n, std::size_t min,
                          std::size_t max) const {
  event::Task task;
  read(&task, out, n, min, max);
  event::wait(manager(), &task);
  return task.result();
}

base::Result Reader::read(std::string* out, std::size_t min,
                          std::size_t max) const {
  event::Task task;
  read(&task, out, min, max);
  event::wait(manager(), &task);
  return task.result();
}

base::Result Reader::write_to(std::size_t* n, std::size_t max,
                              const Writer& w) const {
  event::Task task;
  write_to(&task, n, max, w);
  event::wait(manager(), &task);
  return task.result();
}

base::Result Reader::close() const {
  event::Task task;
  close(&task);
  event::wait(manager(), &task);
  return task.result();
}

namespace {
class FunctionReader : public ReaderImpl {
 public:
  FunctionReader(ReadFn rfn, CloseFn cfn)
      : ReaderImpl(default_options()),
        rfn_(std::move(rfn)),
        cfn_(std::move(cfn)) {}

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) override {
    rfn_(task, out, n, min, max);
  }

  void close(event::Task* task) override { cfn_(task); }

 private:
  ReadFn rfn_;
  CloseFn cfn_;
};

class SyncFunctionReader : public ReaderImpl {
 public:
  SyncFunctionReader(SyncReadFn rfn, SyncCloseFn cfn)
      : ReaderImpl(default_options()),
        rfn_(std::move(rfn)),
        cfn_(std::move(cfn)) {}

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) override {
    if (prologue(task, out, n, min, max)) task->finish(rfn_(out, n, min, max));
  }

  void close(event::Task* task) override {
    if (prologue(task)) task->finish(cfn_());
  }

 private:
  SyncReadFn rfn_;
  SyncCloseFn cfn_;
};

class CloseIgnoringReader : public ReaderImpl {
 public:
  CloseIgnoringReader(Reader r) : ReaderImpl(r.options()), r_(std::move(r)) {}

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) override {
    r_.read(task, out, n, min, max);
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w) override {
    r_.write_to(task, n, max, w);
  }

  void close(event::Task* task) override {
    if (prologue(task)) task->finish_ok();
  }

 private:
  Reader r_;
};

class LimitedReader : public ReaderImpl {
 public:
  LimitedReader(Reader r, std::size_t max)
      : ReaderImpl(r.options()), r_(std::move(r)), remaining_(max) {}

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) override {
    if (!prologue(task, out, n, min, max)) return;

    auto lock = base::acquire_lock(mu_);
    std::size_t amax = std::min(max, remaining_);
    std::size_t amin = std::min(min, remaining_);
    bool eof = (amax < min);

    auto* subtask = new event::Task;
    task->add_subtask(subtask);
    r_.read(subtask, out, n, amin, amax);

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
                const Writer& w) override {
    *n = 0;
    auto* lock = new base::Lock(mu_);
    if (max > remaining_) max = remaining_;
    r_.write_to(task, n, max, w);
    auto closure = [this, n, lock] {
      remaining_ -= *n;
      delete lock;
      return base::Result();
    };
    task->on_finished(event::callback(closure));
  }

  void close(event::Task* task) override { r_.close(task); }

 private:
  Reader r_;
  mutable std::mutex mu_;
  std::size_t remaining_;
};

class StringOrBufferReader : public ReaderImpl {
 public:
  StringOrBufferReader(ConstBuffer buf) noexcept
      : ReaderImpl(default_options()),
        buf_(buf),
        pos_(0),
        closed_(false) {}
  StringOrBufferReader(std::string str) noexcept
      : ReaderImpl(default_options()),
        str_(std::move(str)),
        buf_(ConstBuffer(str_)),
        pos_(0),
        closed_(false) {}

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) override {
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
                const Writer& w) override {
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
    w.write(task, n, ptr, len);
    auto closure = [this, n, lock] {
      pos_ += *n;
      delete lock;
      return base::Result();
    };
    task->on_finished(event::callback(closure));
  }

  void close(event::Task* task) override {
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
  NullReader() noexcept : ReaderImpl(default_options()) {}

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) override {
    if (!prologue(task, out, n, min, max)) return;
    base::Result r;
    if (min > 0) r = base::Result::eof();
    *n = 0;
    task->finish(std::move(r));
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w) override {
    if (!prologue(task, n, max, w)) return;
    *n = 0;
    task->finish_ok();
  }

  void close(event::Task* task) override {
    if (prologue(task)) task->finish_ok();
  }
};

class ZeroReader : public ReaderImpl {
 public:
  ZeroReader() noexcept : ReaderImpl(default_options()) {}

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) override {
    if (!prologue(task, out, n, min, max)) return;
    if (max > 0) ::bzero(out, max);
    *n = max;
    task->finish_ok();
  }

  void close(event::Task* task) override {
    if (prologue(task)) task->finish_ok();
  }
};

class FDReader : public ReaderImpl {
 public:
  struct Op {
    virtual ~Op() noexcept = default;
    virtual bool process(base::Lock& lock, FDReader* reader) = 0;
  };

  struct ReadOp : public Op {
    event::Task* const task;
    char* const out;
    std::size_t* const n;
    const std::size_t min;
    const std::size_t max;

    ReadOp(event::Task* t, char* o, std::size_t* n, std::size_t mn,
           std::size_t mx) noexcept : task(t),
                                      out(o),
                                      n(n),
                                      min(mn),
                                      max(mx) {}
    bool process(base::Lock& lock, FDReader* reader) override;
  };

  struct WriteToOp : public Op {
    event::Task* const task;
    std::size_t* const n;
    const std::size_t max;
    const Writer w;
    event::FileDescriptor wrevt;

    WriteToOp(event::Task* t, std::size_t* n, std::size_t mx, Writer w) noexcept
        : task(t),
          n(n),
          max(mx),
          w(std::move(w)) {}
    bool process(base::Lock& lock, FDReader* reader) override;
  };

  FDReader(base::FD fd, Options o) noexcept : ReaderImpl(std::move(o)),
                                              fd_(std::move(fd)) {}

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) override {
    if (!prologue(task, out, n, min, max)) return;
    auto lock = base::acquire_lock(mu_);
    q_.emplace_back(new ReadOp(task, out, n, min, max));
    VLOG(6) << "io::FDReader::read";
    process(lock);
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w) override {
    if (!prologue(task, n, max, w)) return;
    auto lock = base::acquire_lock(mu_);
    q_.emplace_back(new WriteToOp(task, n, max, w));
    VLOG(6) << "io::FDReader::write_to";
    process(lock);
  }

  void close(event::Task* task) override {
    if (prologue(task)) task->finish(fd_->close());
  }

  void process(base::Lock& lock) {
    VLOG(4) << "io::FDReader::process: q.size()=" << q_.size();
    while (!q_.empty()) {
      std::unique_ptr<Op> op = std::move(q_.front());
      q_.pop_front();
      if (!op->process(lock, this)) {
        q_.push_front(std::move(op));
        break;
      }
      VLOG(5) << "io::FDReader::process: consumed";
    }
  }

  base::Result wake(event::Data data) {
    VLOG(6) << "woke io::FDReader, set=" << data.events;
    auto lock = base::acquire_lock(mu_);
    process(lock);
    return base::Result();
  }

  base::Result arm(base::Lock& lock) {
    base::Result r;
    if (!rdevt_) {
      auto closure = [this](event::Data data) { return wake(data); };
      auto m = options().manager();
      r = m.fd(&rdevt_, fd_, event::Set::readable_bit(),
               event::handler(closure));
    }
    return r;
  }

  base::Result arm_write(event::FileDescriptor* wrevt, const base::FD& wfd,
                         const io::Options& o) {
    base::Result r;
    if (!*wrevt) {
      auto closure = [this](event::Data data) { return wake(data); };
      auto m = o.manager();
      r = m.fd(wrevt, wfd, event::Set::writable_bit(), event::handler(closure));
    }
    return r;
  }

 private:
  const base::FD fd_;
  mutable std::mutex mu_;
  event::FileDescriptor rdevt_;
  std::deque<std::unique_ptr<Op>> q_;
};

bool FDReader::ReadOp::process(base::Lock& lock, FDReader* reader) {
  VLOG(6) << "io::FDReader::ReadOp: begin: "
          << "*n=" << *n << ", "
          << "min=" << min << ", "
          << "max=" << max;

  // Check for cancellation
  if (!task->is_running()) {
    VLOG(6) << "io::FDReader::ReadOp: cancel";
    lock.unlock();
    auto cleanup = base::cleanup([&lock] { lock.lock(); });
    task->finish_cancel();
    return true;
  }

  base::Result r;
  while (*n < max) {
    // Attempt to read some data
    auto pair = reader->fd_->acquire_fd();
    VLOG(4) << "io::FDReader::ReadOp: read: "
            << "fd=" << pair.first << ", "
            << "*n=" << *n << ", "
            << "max=" << max;
    ssize_t len = ::read(pair.first, out + *n, max - *n);
    int err_no = errno;
    VLOG(5) << "result=" << len;
    pair.second.unlock();

    // Check the return code
    if (len < 0) {
      // Interrupted by signal? Retry immediately
      if (err_no == EINTR) {
        VLOG(5) << "EINTR";
        continue;
      }

      // No data for non-blocking read?
      // |min == 0| is success, otherwise reschedule for later
      if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
        VLOG(5) << "EAGAIN";

        // If we've hit the minimum threshold, call it a day.
        if (*n >= min) break;

        // Register a callback for poll, if we didn't already.
        r = reader->arm(lock);
        if (!r) break;
        return false;
      }

      // Other error? Bomb out
      r = base::Result::from_errno(err_no, "read(2)");
      break;
    }
    if (len == 0) {
      if (*n < min) r = base::Result::eof();
      VLOG(4) << "io::FDReader::ReadOp: EOF, " << r;
      break;
    }
    *n += len;
  }
  VLOG(6) << "io::FDReader::ReadOp: finish: "
          << "*n=" << *n << ", "
          << "r=" << r;
  lock.unlock();
  auto cleanup = base::cleanup([&lock] { lock.lock(); });
  task->finish(std::move(r));
  return true;
}

bool FDReader::WriteToOp::process(base::Lock& lock, FDReader* reader) {
  VLOG(6) << "io::FDReader::WriteToOp: begin: "
          << "*n=" << *n << ", "
          << "max=" << max;

  // Check for cancellation
  if (!task->is_running()) {
    VLOG(6) << "io::FDReader::WriteToOp: cancel";
    lock.unlock();
    auto cleanup = base::cleanup([&lock] { lock.lock(); });
    task->finish_cancel();
    return true;
  }

  auto xm = transfer_mode(reader->options(), w.options());
  base::FD rfd = reader->fd_;
  base::FD wfd = w.implementation()->internal_writerfd();
  base::Result r;

  // Try using splice(2)
  if (xm >= TransferMode::splice && wfd) {
    while (*n < max) {
      std::size_t cmax = max - *n;
      if (cmax > kSpliceMax) cmax = kSpliceMax;

      auto pair0 = wfd->acquire_fd();
      auto pair1 = rfd->acquire_fd();
      VLOG(4) << "io::FDReader::WriteToOp: splice: "
              << "wfd=" << pair0.first << ", "
              << "rfd=" << pair1.first << ", "
              << "max=" << cmax << ", "
              << "*n=" << *n;
      ssize_t sent = ::splice(pair1.first, nullptr, pair0.first, nullptr, cmax,
                              SPLICE_F_NONBLOCK);
      int err_no = errno;
      VLOG(5) << "result=" << sent;
      pair1.second.unlock();
      pair0.second.unlock();

      // Check the return code
      if (sent < 0) {
        // Interrupted by signal? Retry immediately
        if (err_no == EINTR) {
          VLOG(5) << "EINTR";
          continue;
        }

        // splice(2) not implemented, at all?
        if (err_no == ENOSYS) {
          VLOG(5) << "ENOSYS";
          goto no_splice;
        }
        // splice(2) not implemented, for this pair of fds?
        if (err_no == EINVAL) {
          VLOG(5) << "EINVAL";
          goto no_splice;
        }

        // No data for read, or full buffers for write?
        // Reschedule for later
        if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
          VLOG(5) << "EAGAIN";

          // Errno doesn't distinguish "reader is empty" from "writer is
          // full", so schedule on both of them.
          r = reader->arm(lock);
          if (!r) break;
          r = reader->arm_write(&wrevt, wfd, w.options());
          if (!r) break;
          return false;
        }

        // Other error? Bomb out
        r = base::Result::from_errno(err_no, "sendfile(2)");
        break;
      }
      *n += sent;
      if (sent == 0) break;
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
      VLOG(4) << "io::FDReader::WriteToOp: sendfile: "
              << "wfd=" << pair0.first << ", "
              << "rfd=" << pair1.first << ", "
              << "max=" << cmax << ", "
              << "*n=" << *n;
      ssize_t sent = ::sendfile(pair0.first, pair1.first, nullptr, cmax);
      int err_no = errno;
      VLOG(5) << "result=" << sent;
      pair1.second.unlock();
      pair0.second.unlock();

      // Check the return code
      if (sent < 0) {
        // Interrupted by signal? Retry immediately
        if (err_no == EINTR) {
          VLOG(5) << "EINTR";
          continue;
        }

        // sendfile(2) not implemented, at all?
        if (err_no == ENOSYS) {
          VLOG(5) << "ENOSYS";
          goto no_sendfile;
        }
        // sendfile(2) not implemented, for this pair of fds?
        if (err_no == EINVAL) {
          VLOG(5) << "EINVAL";
          goto no_sendfile;
        }

        // No data for read, or full buffers for write?
        // Reschedule for later
        if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
          VLOG(5) << "EAGAIN";

          // Errno doesn't distinguish "reader is empty" from "writer is
          // full", so schedule on both of them.
          r = reader->arm(lock);
          if (!r) break;
          r = reader->arm_write(&wrevt, wfd, w.options());
          if (!r) break;
          return false;
        }

        // Other error? Bomb out
        r = base::Result::from_errno(err_no, "sendfile(2)");
        VLOG(5) << "errno=" << err_no << ", " << r.as_string();
        break;
      }
      *n += sent;
      if (sent == 0) break;
    }
    goto finish;
  }
no_sendfile:

  // Nothing else left to try
  r = base::Result::not_implemented();

finish:
  VLOG(6) << "io::FDReader::WriteToOp: finish: "
          << "*n=" << *n << ", "
          << "r=" << r;
  lock.unlock();
  auto cleanup = base::cleanup([&lock] { lock.lock(); });
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
    event::Task subtask;
    std::size_t subn;

    Op(event::Task* t, char* o, std::size_t* n, std::size_t mn,
       std::size_t mx) noexcept : task(t),
                                  out(o),
                                  n(n),
                                  min(mn),
                                  max(mx),
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

  explicit MultiReader(std::vector<Reader> vec, Options o) noexcept
      : ReaderImpl(std::move(o)),
        vec_(std::move(vec)),
        pass_(0),
        curr_(0) {}

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) override {
    if (!prologue(task, out, n, min, max)) return;
    auto lock = base::acquire_lock(mu_);
    q_.emplace_back(new Op(task, out, n, min, max));
    VLOG(6) << "io::MultiReader::read";
    process(lock);
  }

  void close(event::Task* task) override {
    std::size_t size = vec_.size();
    auto* helper = new CloseHelper(task, size);
    auto closure = [helper] { return helper->run(); };
    event::Task* st = helper->subtasks.get();
    for (std::size_t i = 0; i < size; ++i) {
      vec_[i].close(&st[i]);
      st[i].on_finished(event::callback(closure));
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

      bool complete = false;
      while (pass_ > 0) {
        lock.unlock();
        auto cleanup1 = base::cleanup([&lock] { lock.lock(); });
        complete = op->process(this);
        cleanup1.run();
        if (complete) break;
        --pass_;
      }
      pass_ = 1;
      if (!complete) {
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
  r.read(&subtask, subout, &subn, submin, submax);

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

Reader fdreader(base::FD fd, Options o) {
  return Reader(std::make_shared<FDReader>(std::move(fd), std::move(o)));
}

Reader multireader(std::vector<Reader> readers, Options o) {
  return Reader(
      std::make_shared<MultiReader>(std::move(readers), std::move(o)));
}

}  // namespace io
