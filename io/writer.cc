// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/writer.h"

#include <unistd.h>

#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "base/cleanup.h"
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
                           const Reader& r, const Options& opts) {
  if (prologue(task, n, max, r)) task->finish(base::Result::not_implemented());
}

void Writer::assert_valid() const {
  if (!ptr_) {
    LOG(FATAL) << "BUG: io::Writer is empty!";
  }
}

base::Result Writer::write(std::size_t* n, const char* ptr, std::size_t len,
                           const Options& opts) const {
  event::Task task;
  write(&task, n, ptr, len, opts);
  event::wait(opts.manager(), &task);
  return task.result();
}

base::Result Writer::write(std::size_t* n, const std::string& str,
                           const Options& opts) const {
  event::Task task;
  write(&task, n, str.data(), str.size(), opts);
  event::wait(opts.manager(), &task);
  return task.result();
}

base::Result Writer::read_from(std::size_t* n, std::size_t max, const Reader& r,
                               const Options& opts) const {
  event::Task task;
  read_from(&task, n, max, r, opts);
  event::wait(opts.manager(), &task);
  return task.result();
}

base::Result Writer::close(const Options& opts) const {
  event::Task task;
  close(&task, opts);
  event::wait(opts.manager(), &task);
  return task.result();
}

namespace {
class FunctionWriter : public WriterImpl {
 public:
  FunctionWriter(WriteFn wfn, CloseFn cfn) noexcept : wfn_(std::move(wfn)),
                                                      cfn_(std::move(cfn)) {}

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const Options& opts) override {
    wfn_(task, n, ptr, len, opts);
  }

  void close(event::Task* task, const Options& opts) override {
    cfn_(task, opts);
  }

 private:
  WriteFn wfn_;
  CloseFn cfn_;
};

class SyncFunctionWriter : public WriterImpl {
 public:
  SyncFunctionWriter(SyncWriteFn wfn, SyncCloseFn cfn) noexcept
      : wfn_(std::move(wfn)),
        cfn_(std::move(cfn)) {}

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const Options& opts) override {
    if (prologue(task, n, ptr, len)) task->finish(wfn_(n, ptr, len, opts));
  }

  void close(event::Task* task, const Options& opts) override {
    if (prologue(task)) task->finish(cfn_(opts));
  }

 private:
  SyncWriteFn wfn_;
  SyncCloseFn cfn_;
};

class CloseIgnoringWriter : public WriterImpl {
 public:
  CloseIgnoringWriter(Writer w) noexcept : w_(std::move(w)) {
    w_.assert_valid();
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const Options& opts) override {
    w_.write(task, n, ptr, len, opts);
  }

  void read_from(event::Task* task, std::size_t* n, std::size_t max,
                 const Reader& r, const Options& opts) override {
    w_.read_from(task, n, max, r, opts);
  }

  void close(event::Task* task, const Options& opts) override {
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
  StringWriter(std::string* str) noexcept : str_(str), closed_(false) {}

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const Options& opts) override {
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

  void close(event::Task* task, const Options& opts) override {
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
  BufferWriter(Buffer buf, std::size_t* n) noexcept : buf_(buf),
                                                      buflen_(n),
                                                      closed_(false) {
    *buflen_ = 0;
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const Options& opts) override {
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
                 const Reader& r, const Options& opts) override {
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

    r.read(task, data, n, 0, size, opts);

    auto closure = [this, n] {
      auto lock = base::acquire_lock(mu_);
      *buflen_ += *n;
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
  DiscardWriter(std::size_t* /*nullable*/ total) noexcept : total_(total) {
    if (total_) *total_ = 0;
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const Options& opts) override {
    if (!prologue(task, n, ptr, len)) return;
    if (total_) *total_ += len;
    *n = len;
    task->finish_ok();
  }

  void close(event::Task* task, const Options& opts) override {
    if (prologue(task)) task->finish_ok();
  }

 private:
  std::size_t* /*nullable*/ total_;
};

class FullWriter : public WriterImpl {
 public:
  FullWriter() noexcept = default;

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const Options& opts) override {
    if (!prologue(task, n, ptr, len)) return;
    *n = 0;
    base::Result r;
    if (len > 0) r = writer_full();
    task->finish(std::move(r));
  }

  void close(event::Task* task, const Options& opts) override {
    if (prologue(task)) task->finish_ok();
  }
};

class FDWriter : public WriterImpl {
 public:
  struct Op {
    virtual ~Op() noexcept = default;
    virtual void cancel() = 0;
    virtual bool process(FDWriter* writer) = 0;
  };

  struct WriteOp : public Op {
    event::Task* const task;
    std::size_t* const n;
    const char* const ptr;
    const std::size_t len;
    const Options options;
    event::FileDescriptor wrevt;

    WriteOp(event::Task* t, std::size_t* n, const char* p, std::size_t l,
            Options opts) noexcept : task(t),
                                     n(n),
                                     ptr(p),
                                     len(l),
                                     options(std::move(opts)) {}
    void cancel() override { task->cancel(); }
    bool process(FDWriter* writer) override;
  };

  explicit FDWriter(base::FD fd) noexcept : fd_(std::move(fd)), depth_(0) {}
  ~FDWriter() noexcept override;

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const Options& opts) override;
  void close(event::Task* task, const Options& opts) override;
  base::FD internal_writerfd() const override { return fd_; }

 private:
  void process(base::Lock& lock);
  base::Result wake(event::Set set);
  base::Result arm(event::FileDescriptor* evt, const base::FD& fd,
                   event::Set set, const Options& o);

  const base::FD fd_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::deque<std::unique_ptr<Op>> q_;         // protected by mu_
  std::vector<event::FileDescriptor> purge_;  // protected by mu_
  std::size_t depth_;                         // protected by mu_
};

FDWriter::~FDWriter() noexcept {
  VLOG(6) << "io::FDWriter::~FDWriter";
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

void FDWriter::write(event::Task* task, std::size_t* n, const char* ptr,
                     std::size_t len, const Options& opts) {
  if (!prologue(task, n, ptr, len)) return;
  auto lock = base::acquire_lock(mu_);
  VLOG(6) << "io::FDWriter::write: len=" << len;
  q_.emplace_back(new WriteOp(task, n, ptr, len, opts));
  process(lock);
}

void FDWriter::close(event::Task* task, const Options& opts) {
  VLOG(6) << "io::FDWriter::close";
  base::Result r = fd_->close();
  if (prologue(task)) task->finish(std::move(r));
}

void FDWriter::process(base::Lock& lock) {
  VLOG(4) << "io::FDWriter::process: begin: q.size()=" << q_.size();

  while (!q_.empty()) {
    auto op = std::move(q_.front());
    q_.pop_front();
    lock.unlock();
    auto cleanup1 = base::cleanup([&lock] { lock.lock(); });
    bool completed = op->process(this);
    cleanup1.run();
    if (!completed) {
      q_.push_front(std::move(op));
      break;
    }
    VLOG(5) << "io::FDWriter::process: consumed";
  }

  VLOG(4) << "io::FDWriter::process: end";
}

base::Result FDWriter::wake(event::Set set) {
  VLOG(6) << "woke io::FDWriter, set=" << set;
  auto lock = base::acquire_lock(mu_);
  ++depth_;
  auto cleanup = base::cleanup([this] {
    --depth_;
    if (depth_ == 0) cv_.notify_all();
  });
  process(lock);
  return base::Result();
}

base::Result FDWriter::arm(event::FileDescriptor* evt, const base::FD& fd,
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

bool FDWriter::WriteOp::process(FDWriter* writer) {
  VLOG(4) << "io::FDWriter::WriteOp: begin: "
          << "*n=" << *n << ", "
          << "len=" << len;

  // Disable any event and make sure someone waits on it.
  auto cleanup = base::cleanup([this, writer] {
    if (wrevt) {
      wrevt.disable().expect_ok(__FILE__, __LINE__);
      auto lock = base::acquire_lock(writer->mu_);
      writer->purge_.push_back(std::move(wrevt));
    }
  });

  // Check for cancellation
  if (!task->is_running()) {
    VLOG(4) << "io::FDWriter::WriteOp: cancel";
    task->finish_cancel();
    return true;
  }

  const auto& wfd = writer->fd_;

  base::Result r;
  // Until we've fulfilled the write operation...
  while (*n < len) {
    // Try to write all the remaining data
    auto pair = wfd->acquire_fd();
    VLOG(6) << "io::FDWriter::WriteOp: write: "
            << "fd=" << pair.first << ", "
            << "len=" << (len - *n);
    ssize_t written = ::write(pair.first, ptr + *n, len - *n);
    int err_no = errno;
    pair.second.unlock();
    VLOG(6) << "io::FDWriter::WriteOp: result=" << written;

    // Check the return code
    if (written < 0) {
      // Interrupted by signal? Retry immediately
      if (err_no == EINTR) {
        VLOG(6) << "io::FDWriter::WriteOp: EINTR";
        continue;
      }

      // No data for non-blocking write? Reschedule for later
      if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
        VLOG(6) << "io::FDWriter::WriteOp: EAGAIN";

        // Register a callback for poll, if we didn't already.
        r = writer->arm(&wrevt, wfd, event::Set::writable_bit(), options);
        if (!r) break;

        cleanup.cancel();
        return false;
      }

      // Other error? Bomb out
      r = base::Result::from_errno(err_no, "write(2)");
      break;
    }

    // Update the bytes written.
    *n += written;
  }
  VLOG(4) << "io::FDWriter::WriteOp: end: "
          << "*n=" << *n << ", "
          << "r=" << r;
  task->finish(std::move(r));
  return true;
}
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

Writer discardwriter(std::size_t* n) {
  return Writer(std::make_shared<DiscardWriter>(n));
}

Writer fullwriter() { return Writer(std::make_shared<FullWriter>()); }

Writer fdwriter(base::FD fd) {
  return Writer(std::make_shared<FDWriter>(std::move(fd)));
}

}  // namespace io
