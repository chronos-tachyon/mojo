// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/writer.h"

#include <unistd.h>

#include <cstring>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "io/reader.h"

static base::Result writer_closed() {
  return base::Result::failed_precondition("io::Writer is closed");
}

static base::Result writer_full() {
  return base::Result::from_errno(ENOSPC, "io::Writer is full");
}

namespace io {

bool WriterImpl::prologue(event::Task* task, std::size_t* n, const char* ptr,
                          std::size_t len) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(n);
  CHECK(len == 0 || ptr != nullptr);
  *n = 0;
  return task->start();
}

bool WriterImpl::prologue(event::Task* task, std::size_t* n, std::size_t max,
                          const Reader& r) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(n);
  r.assert_valid();
  *n = 0;
  return task->start();
}

bool WriterImpl::prologue(event::Task* task) {
  CHECK_NOTNULL(task);
  return task->start();
}

void WriterImpl::read_from(event::Task* task, std::size_t* n, std::size_t max,
                           const Reader& r) {
  if (prologue(task, n, max, r)) task->finish(base::Result::not_implemented());
}

void Writer::assert_valid() const {
  if (!ptr_) {
    LOG(FATAL) << "BUG: io::Writer is empty!";
  }
}

base::Result Writer::write(std::size_t* n, const char* ptr,
                           std::size_t len) const {
  event::Task task;
  write(&task, n, ptr, len);
  event::wait(manager(), &task);
  return task.result();
}

base::Result Writer::write(std::size_t* n, const std::string& str) const {
  event::Task task;
  write(&task, n, str.data(), str.size());
  event::wait(manager(), &task);
  return task.result();
}

base::Result Writer::read_from(std::size_t* n, std::size_t max,
                               const Reader& r) const {
  event::Task task;
  read_from(&task, n, max, r);
  event::wait(manager(), &task);
  return task.result();
}

base::Result Writer::close() const {
  event::Task task;
  close(&task);
  event::wait(manager(), &task);
  return task.result();
}

namespace {
class FunctionWriter : public WriterImpl {
 public:
  FunctionWriter(WriteFn wfn, CloseFn cfn) noexcept
      : WriterImpl(default_options()),
        wfn_(std::move(wfn)),
        cfn_(std::move(cfn)) {}

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len) override {
    wfn_(task, n, ptr, len);
  }

  void close(event::Task* task) override { cfn_(task); }

 private:
  WriteFn wfn_;
  CloseFn cfn_;
};

class SyncFunctionWriter : public WriterImpl {
 public:
  SyncFunctionWriter(SyncWriteFn wfn, SyncCloseFn cfn) noexcept
      : WriterImpl(default_options()),
        wfn_(std::move(wfn)),
        cfn_(std::move(cfn)) {}

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len) override {
    if (prologue(task, n, ptr, len)) task->finish(wfn_(n, ptr, len));
  }

  void close(event::Task* task) override {
    if (prologue(task)) task->finish(cfn_());
  }

 private:
  SyncWriteFn wfn_;
  SyncCloseFn cfn_;
};

class CloseIgnoringWriter : public WriterImpl {
 public:
  CloseIgnoringWriter(Writer w) noexcept : WriterImpl(w.options()),
                                           w_(std::move(w)) {
    w_.assert_valid();
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len) override {
    w_.write(task, n, ptr, len);
  }

  void read_from(event::Task* task, std::size_t* n, std::size_t max,
                 const Reader& r) override {
    w_.read_from(task, n, max, r);
  }

  void close(event::Task* task) override {
    if (prologue(task)) task->finish_ok();
  }

  base::FD internal_writerfd() const override {
    return w_.implementation()->internal_writerfd();
  }

 private:
  Writer w_;
};

class StringWriter : public WriterImpl {
 public:
  StringWriter(std::string* str) noexcept : WriterImpl(default_options()),
                                            str_(str),
                                            closed_(false) {}

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len) override {
    if (!prologue(task, n, ptr, len)) return;
    auto lock = base::acquire_lock(mu_);
    if (closed_) {
      task->finish(writer_closed());
      return;
    }
    str_->append(ptr, ptr + len);
    lock.unlock();
    *n = len;
    task->finish_ok();
  }

  void close(event::Task* task) override {
    auto lock = base::acquire_lock(mu_);
    bool was = closed_;
    closed_ = true;
    lock.unlock();
    if (prologue(task)) {
      if (was)
        task->finish(writer_closed());
      else
        task->finish_ok();
    }
  }

 private:
  std::string* const str_;
  mutable std::mutex mu_;
  bool closed_;
};

class BufferWriter : public WriterImpl {
 public:
  BufferWriter(Buffer buf, std::size_t* n) noexcept
      : WriterImpl(default_options()),
        buf_(buf),
        buflen_(n),
        closed_(false) {
    *buflen_ = 0;
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len) override {
    if (!prologue(task, n, ptr, len)) return;
    auto lock = base::acquire_lock(mu_);
    if (closed_) {
      task->finish(writer_closed());
      return;
    }
    char* data = buf_.data() + *buflen_;
    std::size_t size = buf_.size() - *buflen_;
    if (size > len) size = len;
    ::memcpy(data, ptr, size);
    *buflen_ += size;
    lock.unlock();
    *n = size;
    if (size < len)
      task->finish(writer_full());
    else
      task->finish_ok();
  }

  void read_from(event::Task* task, std::size_t* n, std::size_t max,
                 const Reader& r) override {
    *n = 0;

    auto lock = base::acquire_lock(mu_);
    if (closed_) {
      if (task->start()) task->finish(writer_closed());
      return;
    }
    char* data = buf_.data() + *buflen_;
    std::size_t size = buf_.size() - *buflen_;
    if (size > max) size = max;
    lock.unlock();

    r.read(task, data, n, 0, size);

    auto closure = [this, n] {
      auto lock = base::acquire_lock(mu_);
      *buflen_ += *n;
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
        task->finish(writer_closed());
      else
        task->finish_ok();
    }
  }

 private:
  const Buffer buf_;
  std::size_t* const buflen_;
  mutable std::mutex mu_;
  bool closed_;
};

class DiscardWriter : public WriterImpl {
 public:
  DiscardWriter(std::size_t* n, Options o) noexcept : WriterImpl(std::move(o)),
                                                      n_(n) {
    if (n_) *n_ = 0;
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len) override {
    if (!prologue(task, n, ptr, len)) return;
    if (n_) *n_ += len;
    *n = len;
    task->finish_ok();
  }

  void close(event::Task* task) override {
    if (prologue(task)) task->finish_ok();
  }

 private:
  std::size_t* n_;
};

class FullWriter : public WriterImpl {
 public:
  FullWriter(Options o) noexcept : WriterImpl(std::move(o)) {}

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len) override {
    if (!prologue(task, n, ptr, len)) return;
    *n = 0;
    base::Result r;
    if (len > 0) r = writer_full();
    task->finish(std::move(r));
  }

  void close(event::Task* task) override {
    if (prologue(task)) task->finish_ok();
  }
};

class FDWriter : public WriterImpl {
 public:
  struct WriteOp {
    event::Task* const task;
    std::size_t* const n;
    const char* const ptr;
    const std::size_t len;

    WriteOp(event::Task* t, std::size_t* n, const char* p,
            std::size_t l) noexcept : task(t),
                                      n(n),
                                      ptr(p),
                                      len(l) {}
  };

  FDWriter(base::FD fd, Options o) noexcept : WriterImpl(std::move(o)),
                                              fd_(std::move(fd)) {}

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len) override {
    if (!prologue(task, n, ptr, len)) return;
    auto lock = base::acquire_lock(mu_);
    q_.emplace_back(task, n, ptr, len);
    process(lock);
  }

  void close(event::Task* task) override {
    if (prologue(task)) task->finish(fd_->close());
  }

  base::FD internal_writerfd() const override { return fd_; }

  void process(base::Lock& lock) {
    while (!q_.empty()) {
      if (!process_one(lock, q_.front())) break;
      q_.pop_front();
    }

    // If we emptied out the queue, nothing more to do.
    if (q_.empty()) return;

    // If we already have an event handler, nothing more to do.
    if (fdevt_) return;

    // Finish processing the queue once the FD becomes writable again.
    auto fn = [this](event::Data) {
      auto lock = base::acquire_lock(mu_);
      process(lock);
      return base::Result();
    };
    auto m = options().manager();
    auto r = m.fd(&fdevt_, fd_, event::Set::writable_bit(), event::handler(fn));
    r.expect_ok();

    // If we failed to set up the event handler, give the bad news to all
    // pending write operations.
    if (!r) {
      for (const WriteOp& op : q_) {
        op.task->finish(r);
      }
      q_.clear();
    }
  }

  bool process_one(base::Lock& lock, const WriteOp& op) {
    base::Result r;
    // Until we've fulfilled the write operation...
    while (*op.n < op.len) {
      // Check for cancellation
      if (!op.task->is_running()) {
        op.task->finish_cancel();
        return true;
      }

      // Try to write all the remaining data.
      auto pair = fd_->acquire_fd();
      VLOG(4) << "io::FDWriter::write: "
              << "fd=" << pair.first << ", "
              << "len=" << op.len << ", "
              << "*n=" << *op.n;
      const char* ptr = op.ptr + *op.n;
      std::size_t len = op.len - *op.n;
      ssize_t written = ::write(pair.first, ptr, len);
      int err_no = errno;
      VLOG(5) << "result=" << written;
      pair.second.unlock();

      // Check the return code
      if (written < 0) {
        // Interrupted by signal? Retry immediately
        if (err_no == EINTR) {
          VLOG(4) << "EINTR";
          continue;
        }

        // No data for non-blocking write? Reschedule for later
        if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
          VLOG(4) << "EAGAIN";
          return false;
        }

        // Other error? Bomb out
        r = base::Result::from_errno(err_no, "write(2)");
        VLOG(5) << "errno=" << err_no << ", " << r.as_string();
        break;
      }

      // Update the bytes written.
      *op.n += written;
    }
    op.task->finish(std::move(r));
    return true;
  }

 private:
  base::FD fd_;
  event::FileDescriptor fdevt_;
  mutable std::mutex mu_;
  std::deque<WriteOp> q_;
};
}  // anonymous namespace

Writer writer(WriteFn wfn, CloseFn cfn) {
  return Writer(
      std::make_shared<FunctionWriter>(std::move(wfn), std::move(cfn)));
}

Writer writer(SyncWriteFn wfn, SyncCloseFn cfn) {
  return Writer(
      std::make_shared<SyncFunctionWriter>(std::move(wfn), std::move(cfn)));
}

Writer ignore_close(Writer w) {
  return Writer(std::make_shared<CloseIgnoringWriter>(std::move(w)));
}

Writer stringwriter(std::string* str) {
  return Writer(std::make_shared<StringWriter>(str));
}

Writer bufferwriter(Buffer buf, std::size_t* n) {
  return Writer(std::make_shared<BufferWriter>(buf, n));
}

Writer discardwriter(std::size_t* n, Options o) {
  return Writer(std::make_shared<DiscardWriter>(n, std::move(o)));
}

Writer fullwriter(Options o) {
  return Writer(std::make_shared<FullWriter>(std::move(o)));
}

Writer fdwriter(base::FD fd, Options o) {
  return Writer(std::make_shared<FDWriter>(std::move(fd), std::move(o)));
}

}  // namespace io
