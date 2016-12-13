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
#include <exception>
#include <mutex>
#include <stdexcept>

#include "base/cleanup.h"
#include "base/logging.h"
#include "base/util.h"
#include "io/writer.h"

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

// FIXME: FDReader needs to be re-architected to serialize concurrent reads by
//        diverting them into a queue and processing them in some order.
//        (While |read(2)| is atomic, we call it multiple times, so our use is
//        not thread-safe.)
class FDReader : public ReaderImpl {
 public:
  FDReader(base::FD fd, Options o) noexcept : ReaderImpl(std::move(o)),
                                              fd_(std::move(fd)) {}

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) override {
    if (!prologue(task, out, n, min, max)) return;
    auto* helper = new ReadHelper(task, out, n, min, max, this);
    helper->run();
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w) override {
    if (!prologue(task, n, max, w)) return;

    auto xm = transfer_mode(options(), w.options());
    if (xm < TransferMode::sendfile) {
      task->finish(base::Result::not_implemented());
      return;
    }

    base::FD rfd = fd_;
    base::FD wfd = w.implementation()->internal_writerfd();
    if (!wfd) {
      task->finish(base::Result::not_implemented());
      return;
    }

    event::Manager rmgr = options().manager();
    event::Manager wmgr = w.options().manager();

    auto* helper =
        new WriteToHelper(task, n, max, std::move(wfd), std::move(rfd),
                          std::move(wmgr), std::move(rmgr));
    if (xm >= TransferMode::splice) {
      helper->run_splice();
    } else {
      helper->run_sendfile();
    }
  }

  void close(event::Task* task) override {
    if (prologue(task)) task->finish(fd_->close());
  }

 private:
  struct ReadHelper {
    event::Task* const task;
    char* const out;
    std::size_t* const n;
    const std::size_t min;
    const std::size_t max;
    FDReader* const reader;
    event::FileDescriptor fdevt;

    ReadHelper(event::Task* t, char* p, std::size_t* n, std::size_t mn,
               std::size_t mx, FDReader* r) noexcept : task(t),
                                                       out(p),
                                                       n(n),
                                                       min(mn),
                                                       max(mx),
                                                       reader(r) {}

    base::Result run() {
      base::Result r;
      while (*n < max) {
        // Check for cancellation
        if (!task->is_running()) {
          task->finish_cancel();
          break;
        }

        // Attempt to read some data
        auto pair = reader->fd_->acquire_fd();
        VLOG(4) << "io::FDReader::read: "
                << "fd=" << pair.first << ", "
                << "max=" << max << ", "
                << "*n=" << *n;
        ssize_t len = ::read(pair.first, out + *n, max - *n);
        int err_no = errno;
        VLOG(5) << "result=" << len;
        pair.second.unlock();

        // Check the return code
        if (len < 0) {
          // Interrupted by signal? Retry immediately
          if (err_no == EINTR) {
            VLOG(4) << "EINTR";
            continue;
          }

          // No data for non-blocking read?
          // |min == 0| is success, otherwise reschedule for later
          if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
            VLOG(4) << "EAGAIN";

            // If we've hit the minimum threshold, call it a day.
            if (*n >= min) break;

            // Register a callback for poll, if we didn't already.
            auto closure = [this](event::Data data) {
              VLOG(5) << "woke io::FDReader::read, set=" << data.events;
              return run();
            };
            if (!fdevt) {
              r = reader->options().manager().fd(&fdevt, reader->fd_,
                                                 event::Set::readable_bit(),
                                                 event::handler(closure));
              if (!r) break;
            }
            return base::Result();
          }

          // Other error? Bomb out
          r = base::Result::from_errno(err_no, "read(2)");
          VLOG(5) << "errno=" << err_no << ", " << r.as_string();
          break;
        }
        if (len == 0) {
          if (*n < min) r = base::Result::eof();
          VLOG(4) << "EOF, " << r.as_string();
          break;
        }
        *n += len;
      }
      task->finish(std::move(r));
      delete this;
      return base::Result();
    }
  };

  struct WriteToHelper {
    event::Task* const task;
    std::size_t* const n;
    std::size_t const max;
    const base::FD wfd;
    const base::FD rfd;
    const event::Manager wmgr;
    const event::Manager rmgr;
    event::FileDescriptor wevt;
    event::FileDescriptor revt;

    WriteToHelper(event::Task* t, std::size_t* n, std::size_t mx, base::FD wd,
                  base::FD rd, event::Manager wm, event::Manager rm) noexcept
        : task(t),
          n(n),
          max(mx),
          wfd(std::move(wd)),
          rfd(std::move(rd)),
          wmgr(std::move(wm)),
          rmgr(std::move(rm)) {}

    base::Result run_fallback() {
      task->finish(base::Result::not_implemented());
      delete this;
      return base::Result();
    }

    base::Result run_sendfile() {
      base::Result r;
      while (*n < max) {
        std::size_t cmax = max - *n;
        if (cmax > kSendfileMax) cmax = kSendfileMax;

        auto pair0 = wfd->acquire_fd();
        auto pair1 = rfd->acquire_fd();
        VLOG(4) << "io::FDReader::write_to: sendfile: "
                << "wfd=" << pair0.first << ", "
                << "rfd=" << pair1.first << ", "
                << "max=" << max << ", "
                << "*n=" << *n << ", "
                << "cmax=" << cmax;
        ssize_t sent = ::sendfile(pair0.first, pair1.first, nullptr, cmax);
        int err_no = errno;
        VLOG(5) << "result=" << sent;
        pair1.second.unlock();
        pair0.second.unlock();

        // Check the return code
        if (sent < 0) {
          // Interrupted by signal? Retry immediately
          if (err_no == EINTR) {
            VLOG(4) << "EINTR";
            continue;
          }

          // sendfile(2) not implemented, at all or for this pair of fds?
          // Switch to fallback mode
          if (err_no == ENOSYS) {
            VLOG(4) << "ENOSYS";
            return run_fallback();
          }
          if (err_no == EINVAL) {
            VLOG(4) << "EINVAL";
            return run_fallback();
          }

          // No data for read, or full buffers for write?
          // Reschedule for later
          if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
            VLOG(4) << "EAGAIN";

            // Errno doesn't distinguish "reader is empty" from "writer is
            // full", so schedule on both of them.
            auto closure = [this](event::Data data) {
              VLOG(5) << "woke io::FDReader::write_to, set=" << data.events;
              return run_sendfile();
            };
            if (!wevt) {
              r = wmgr.fd(&wevt, wfd, event::Set::writable_bit(),
                          event::handler(closure));
              if (!r) break;
              r = rmgr.fd(&revt, rfd, event::Set::readable_bit(),
                          event::handler(closure));
              if (!r) break;
            }
            return base::Result();
          }

          // Other error? Bomb out
          r = base::Result::from_errno(err_no, "sendfile(2)");
          VLOG(5) << "errno=" << err_no << ", " << r.as_string();
          break;
        }
        if (sent == 0) break;
        *n += sent;
      }
      task->finish(std::move(r));
      delete this;
      return base::Result();
    }

    base::Result run_splice() {
      base::Result r;
      while (*n < max) {
        std::size_t cmax = max - *n;
        if (cmax > kSendfileMax) cmax = kSendfileMax;

        auto pair0 = wfd->acquire_fd();
        auto pair1 = rfd->acquire_fd();
        VLOG(4) << "io::FDReader::write_to: splice: "
                << "wfd=" << pair0.first << ", "
                << "rfd=" << pair1.first << ", "
                << "max=" << max << ", "
                << "*n=" << *n << ", "
                << "cmax=" << cmax;
        ssize_t sent = ::splice(pair1.first, nullptr, pair0.first, nullptr,
                                cmax, SPLICE_F_NONBLOCK);
        int err_no = errno;
        VLOG(5) << "result=" << sent;
        pair1.second.unlock();
        pair0.second.unlock();

        // Check the return code
        if (sent < 0) {
          // Interrupted by signal? Retry immediately
          if (err_no == EINTR) {
            VLOG(4) << "EINTR";
            continue;
          }

          // splice(2) not implemented, at all or for this pair of fds?
          // Switch to sendfile(2) mode
          if (err_no == ENOSYS) {
            VLOG(4) << "ENOSYS";
            return run_sendfile();
          }
          if (err_no == EINVAL) {
            VLOG(4) << "EINVAL";
            return run_sendfile();
          }

          // No data for read, or full buffers for write?
          // Reschedule for later
          if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
            VLOG(4) << "EAGAIN";

            // Errno doesn't distinguish "reader is empty" from "writer is
            // full", so schedule on both of them.
            auto closure = [this](event::Data data) {
              VLOG(5) << "woke io::FDReader::write_to, set=" << data.events;
              return run_splice();
            };
            if (!wevt) {
              r = wmgr.fd(&wevt, wfd, event::Set::writable_bit(),
                          event::handler(closure));
              if (!r) break;
              r = rmgr.fd(&revt, rfd, event::Set::readable_bit(),
                          event::handler(closure));
              if (!r) break;
            }
            return base::Result();
          }

          // Other error? Bomb out
          r = base::Result::from_errno(err_no, "sendfile(2)");
          VLOG(5) << "errno=" << err_no << ", " << r.as_string();
          break;
        }
        if (sent == 0) break;
        *n += sent;
      }
      task->finish(std::move(r));
      delete this;
      return base::Result();
    }
  };

 private:
  base::FD fd_;
};
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

}  // namespace io
