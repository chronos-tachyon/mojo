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

#include "base/backport.h"
#include "base/cleanup.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "io/buffer.h"
#include "io/chain.h"
#include "io/reader.h"

namespace io {

bool WriterImpl::prologue(event::Task* task, std::size_t* n, const char* ptr,
                          std::size_t len) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(n);
  CHECK_GE(len, 0U);
  if (len > 0) CHECK_NOTNULL(ptr);

  bool start = task->start();
  if (start) *n = 0;
  return start;
}

bool WriterImpl::prologue(event::Task* task, std::size_t* n, std::size_t max,
                          const Reader& r) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(n);
  CHECK_GE(max, 0U);
  r.assert_valid();

  bool start = task->start();
  if (start) *n = 0;
  return start;
}

bool WriterImpl::prologue(event::Task* task) {
  CHECK_NOTNULL(task);
  return task->start();
}

void WriterImpl::read_from(event::Task* task, std::size_t* n, std::size_t max,
                           const Reader& r, const base::Options& opts) {
  if (prologue(task, n, max, r)) task->finish(base::Result::not_implemented());
}

void WriterImpl::flush(event::Task* task, const base::Options& opts) {
  if (prologue(task)) task->finish_ok();
}

void WriterImpl::sync(event::Task* task, const base::Options& opts) {
  flush(task, opts);
}

void Writer::assert_valid() const { CHECK(ptr_) << ": io::Writer is empty!"; }

inline namespace implementation {
struct WriteFixedHelper : public event::Callback {
  event::Task subtask;
  event::Task* const task;
  char buf[10];
  std::size_t n;

  explicit WriteFixedHelper(event::Task* t) noexcept : task(t), n(0) {}

  base::Result run() override {
    event::propagate_result(task, &subtask);
    return base::Result();
  }
};
}  // inline namespace implementation

void Writer::write_u8(event::Task* task, uint8_t in,
                      const base::Options& opts) const {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  auto helper = base::backport::make_unique<WriteFixedHelper>(task);
  auto* h = helper.get();
  unsigned char* p = reinterpret_cast<unsigned char*>(&h->buf);
  p[0] = in;
  task->add_subtask(&h->subtask);
  write(&h->subtask, &h->n, h->buf, 1, opts);
  h->subtask.on_finished(std::move(helper));
}

void Writer::write_u16(event::Task* task, uint16_t in,
                       const base::Endian* endian,
                       const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(endian);
  if (!task->start()) return;

  auto helper = base::backport::make_unique<WriteFixedHelper>(task);
  auto* h = helper.get();
  endian->put_u16(h->buf, in);
  task->add_subtask(&h->subtask);
  write(&h->subtask, &h->n, h->buf, 2, opts);
  h->subtask.on_finished(std::move(helper));
}

void Writer::write_u32(event::Task* task, uint32_t in,
                       const base::Endian* endian,
                       const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(endian);
  if (!task->start()) return;

  auto helper = base::backport::make_unique<WriteFixedHelper>(task);
  auto* h = helper.get();
  endian->put_u32(h->buf, in);
  task->add_subtask(&h->subtask);
  write(&h->subtask, &h->n, h->buf, 4, opts);
  h->subtask.on_finished(std::move(helper));
}

void Writer::write_u64(event::Task* task, uint64_t in,
                       const base::Endian* endian,
                       const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(endian);
  if (!task->start()) return;

  auto helper = base::backport::make_unique<WriteFixedHelper>(task);
  auto* h = helper.get();
  endian->put_u64(h->buf, in);
  task->add_subtask(&h->subtask);
  write(&h->subtask, &h->n, h->buf, 8, opts);
  h->subtask.on_finished(std::move(helper));
}

void Writer::write_s8(event::Task* task, int8_t in,
                      const base::Options& opts) const {
  uint8_t tmp;
  if (in < 0)
    tmp = ~uint8_t(-(in + 1));
  else
    tmp = in;
  write_u8(task, tmp, opts);
}

void Writer::write_s16(event::Task* task, int16_t in,
                       const base::Endian* endian,
                       const base::Options& opts) const {
  uint16_t tmp;
  if (in < 0)
    tmp = ~uint16_t(-(in + 1));
  else
    tmp = in;
  write_u16(task, tmp, endian, opts);
}

void Writer::write_s32(event::Task* task, int32_t in,
                       const base::Endian* endian,
                       const base::Options& opts) const {
  uint32_t tmp;
  if (in < 0)
    tmp = ~uint32_t(-(in + 1));
  else
    tmp = in;
  write_u32(task, tmp, endian, opts);
}

void Writer::write_s64(event::Task* task, int64_t in,
                       const base::Endian* endian,
                       const base::Options& opts) const {
  uint64_t tmp;
  if (in < 0)
    tmp = ~uint64_t(-(in + 1));
  else
    tmp = in;
  write_u64(task, tmp, endian, opts);
}

void Writer::write_uvarint(event::Task* task, uint64_t in,
                           const base::Options& opts) const {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  auto helper = base::backport::make_unique<WriteFixedHelper>(task);
  auto* h = helper.get();
  unsigned char* p = reinterpret_cast<unsigned char*>(&h->buf);
  std::size_t len = 0;
  while (in >= 0x80) {
    p[len] = 0x80 | (in & 0x7f);
    in >>= 7;
    ++len;
  }
  p[len] = (in & 0x7f);
  ++len;
  task->add_subtask(&h->subtask);
  write(&h->subtask, &h->n, h->buf, len, opts);
  h->subtask.on_finished(std::move(helper));
}

void Writer::write_svarint(event::Task* task, int64_t in,
                           const base::Options& opts) const {
  uint64_t tmp;
  if (in < 0)
    tmp = ~uint64_t(-(in + 1));
  else
    tmp = in;
  write_uvarint(task, tmp, opts);
}

void Writer::write_svarint_zigzag(event::Task* task, int64_t in,
                                  const base::Options& opts) const {
  uint64_t tmp;
  if (in < 0)
    tmp = (uint64_t(-(in + 1)) << 1) + 1;
  else
    tmp = uint64_t(in) << 1;
  write_uvarint(task, tmp, opts);
}

base::Result Writer::write(std::size_t* n, const char* ptr, std::size_t len,
                           const base::Options& opts) const {
  event::Task task;
  write(&task, n, ptr, len, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write(std::size_t* n, const std::string& str,
                           const base::Options& opts) const {
  event::Task task;
  write(&task, n, str.data(), str.size(), opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write_u8(uint8_t in, const base::Options& opts) const {
  event::Task task;
  write_u8(&task, in, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write_u16(uint16_t in, const base::Endian* endian,
                               const base::Options& opts) const {
  event::Task task;
  write_u16(&task, in, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write_u32(uint32_t in, const base::Endian* endian,
                               const base::Options& opts) const {
  event::Task task;
  write_u32(&task, in, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write_u64(uint64_t in, const base::Endian* endian,
                               const base::Options& opts) const {
  event::Task task;
  write_u64(&task, in, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write_s8(int8_t in, const base::Options& opts) const {
  event::Task task;
  write_s8(&task, in, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write_s16(int16_t in, const base::Endian* endian,
                               const base::Options& opts) const {
  event::Task task;
  write_s16(&task, in, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write_s32(int32_t in, const base::Endian* endian,
                               const base::Options& opts) const {
  event::Task task;
  write_s32(&task, in, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write_s64(int64_t in, const base::Endian* endian,
                               const base::Options& opts) const {
  event::Task task;
  write_s64(&task, in, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write_uvarint(uint64_t in,
                                   const base::Options& opts) const {
  event::Task task;
  write_uvarint(&task, in, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write_svarint(int64_t in,
                                   const base::Options& opts) const {
  event::Task task;
  write_svarint(&task, in, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::write_svarint_zigzag(int64_t in,
                                          const base::Options& opts) const {
  event::Task task;
  write_svarint_zigzag(&task, in, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::read_from(std::size_t* n, std::size_t max, const Reader& r,
                               const base::Options& opts) const {
  event::Task task;
  read_from(&task, n, max, r, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::flush(const base::Options& opts) const {
  event::Task task;
  flush(&task, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::sync(const base::Options& opts) const {
  event::Task task;
  sync(&task, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Writer::close(const base::Options& opts) const {
  event::Task task;
  close(&task, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

inline namespace implementation {
class FunctionWriter : public WriterImpl {
 public:
  FunctionWriter(WriteFn wfn, CloseFn cfn) noexcept : wfn_(std::move(wfn)),
                                                      cfn_(std::move(cfn)) {}

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
    wfn_(task, n, ptr, len, opts);
  }

  void close(event::Task* task, const base::Options& opts) override {
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

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
    if (prologue(task, n, ptr, len)) task->finish(wfn_(n, ptr, len, opts));
  }

  void close(event::Task* task, const base::Options& opts) override {
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

  std::size_t ideal_block_size() const noexcept override {
    return w_.ideal_block_size();
  }

  bool is_buffered() const noexcept override { return w_.is_buffered(); }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
    w_.write(task, n, ptr, len, opts);
  }

  void read_from(event::Task* task, std::size_t* n, std::size_t max,
                 const Reader& r, const base::Options& opts) override {
    w_.read_from(task, n, max, r, opts);
  }

  void flush(event::Task* task, const base::Options& opts) override {
    w_.flush(task, opts);
  }

  void sync(event::Task* task, const base::Options& opts) override {
    w_.sync(task, opts);
  }

  void close(event::Task* task, const base::Options& opts) override {
    w_.sync(task, opts);
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

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  bool is_buffered() const noexcept override { return true; }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
    if (!prologue(task, n, ptr, len)) return;

    auto lock = base::acquire_lock(mu_);
    if (closed_) {
      task->finish(writer_closed());
      return;
    }
    str_->append(ptr, len);
    lock.unlock();

    *n = len;
    task->finish_ok();
  }

  void close(event::Task* task, const base::Options& opts) override {
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

  ~BufferWriter() noexcept override { mu_.lock(); }

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  bool is_buffered() const noexcept override { return true; }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
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
                 const Reader& r, const base::Options& opts) override {
    if (!prologue(task, n, max, r)) return;

    auto lock = base::acquire_lock(mu_);
    if (closed_) {
      task->finish(writer_closed());
      return;
    }

    char* data = buf_.data() + *buflen_;
    std::size_t size = buf_.size() - *buflen_;
    if (size > max) size = max;

    auto helper =
        base::backport::make_unique<Helper>(task, n, buflen_, std::move(lock));
    auto* st = &helper->subtask;
    r.read(st, data, n, 0, size, opts);
    st->on_finished(std::move(helper));
  }

  void close(event::Task* task, const base::Options& opts) override {
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
  struct Helper : public event::Callback {
    event::Task* const task;
    std::size_t* const n;
    std::size_t* const buflen;
    base::Lock lock;
    event::Task subtask;

    Helper(event::Task* t, std::size_t* n, std::size_t* l,
           base::Lock lk) noexcept : task(t),
                                     n(n),
                                     buflen(l),
                                     lock(std::move(lk)) {
      task->add_subtask(&subtask);
    }

    base::Result run() override {
      *buflen += *n;
      lock.unlock();
      event::propagate_result(task, &subtask);
      return base::Result();
    }
  };

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

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  bool is_buffered() const noexcept override { return true; }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
    if (!prologue(task, n, ptr, len)) return;
    if (total_) *total_ += len;
    *n = len;
    task->finish_ok();
  }

  void close(event::Task* task, const base::Options& opts) override {
    if (prologue(task)) task->finish_ok();
  }

 private:
  std::size_t* /*nullable*/ total_;
};

class FullWriter : public WriterImpl {
 public:
  FullWriter() noexcept = default;

  std::size_t ideal_block_size() const noexcept override { return 64; }

  bool is_buffered() const noexcept override { return true; }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
    if (!prologue(task, n, ptr, len)) return;
    base::Result r;
    if (len > 0) r = writer_full();
    task->finish(std::move(r));
  }

  void close(event::Task* task, const base::Options& opts) override {
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
    const base::Options options;
    event::Handle wrevt;

    WriteOp(event::Task* t, std::size_t* n, const char* p, std::size_t l,
            base::Options opts) noexcept : task(t),
                                           n(n),
                                           ptr(p),
                                           len(l),
                                           options(std::move(opts)) {}
    void cancel() override { task->cancel(); }
    bool process(FDWriter* writer) override;
  };

  struct SyncOp : public Op {
    event::Task* const task;

    explicit SyncOp(event::Task* t) noexcept : task(t) {}
    void cancel() override { task->cancel(); }
    bool process(FDWriter* writer) override;
  };

  struct CloseOp : public Op {
    event::Task* const task;

    explicit CloseOp(event::Task* t) noexcept : task(t) {}
    void cancel() override { task->cancel(); }
    bool process(FDWriter* writer) override;
  };

  explicit FDWriter(base::FD fd) noexcept : fd_(std::move(fd)), depth_(0) {}
  ~FDWriter() noexcept override;

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override;
  void sync(event::Task* task, const base::Options& opts) override;
  void close(event::Task* task, const base::Options& opts) override;
  base::FD internal_writerfd() const override { return fd_; }

 private:
  void process(base::Lock& lock);
  base::Result wake(event::Set set);
  base::Result arm(event::Handle* evt, const base::FD& fd, event::Set set,
                   const base::Options& o);

  const base::FD fd_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::deque<std::unique_ptr<Op>> q_;  // protected by mu_
  std::vector<event::Handle> purge_;   // protected by mu_
  std::size_t depth_;                  // protected by mu_
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
                     std::size_t len, const base::Options& opts) {
  if (!prologue(task, n, ptr, len)) return;
  auto lock = base::acquire_lock(mu_);
  VLOG(6) << "io::FDWriter::write: len=" << len;
  q_.emplace_back(new WriteOp(task, n, ptr, len, opts));
  process(lock);
}

void FDWriter::sync(event::Task* task, const base::Options& opts) {
  if (!prologue(task)) return;
  auto lock = base::acquire_lock(mu_);
  VLOG(6) << "io::FDWriter::sync";
  q_.emplace_back(new SyncOp(task));
  process(lock);
}

void FDWriter::close(event::Task* task, const base::Options& opts) {
  if (!prologue(task)) return;
  auto lock = base::acquire_lock(mu_);
  VLOG(6) << "io::FDWriter::close";
  q_.emplace_back(new CloseOp(task));
  process(lock);
}

void FDWriter::process(base::Lock& lock) {
  VLOG(4) << "io::FDWriter::process: begin: q.size()=" << q_.size();

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
    VLOG(5) << "io::FDWriter::process: consumed";
  }

  if (event::internal::is_shallow()) {
    auto p = std::move(purge_);
    for (auto& evt : p) evt.wait();
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

base::Result FDWriter::arm(event::Handle* evt, const base::FD& fd,
                           event::Set set, const base::Options& o) {
  DCHECK_NOTNULL(evt);
  base::Result r;
  if (!*evt) {
    event::Manager manager = get_manager(o);
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

bool FDWriter::SyncOp::process(FDWriter* writer) {
  VLOG(4) << "io::FDWriter::SyncOp: begin";

  // Check for cancellation
  if (!task->is_running()) {
    VLOG(4) << "io::FDWriter::SyncOp: cancel";
    task->finish_cancel();
    return true;
  }

  auto fdpair = writer->fd_->acquire_fd();
  int rc = ::fdatasync(fdpair.first);
  int err_no = errno;
  fdpair.second.unlock();

  base::Result r;
  if (rc != 0) {
    r = base::Result::from_errno(err_no, "fdatasync(2)");
  }

  VLOG(4) << "io::FDWriter::SyncOp: end: "
          << "r=" << r;
  task->finish(std::move(r));
  return true;
}

bool FDWriter::CloseOp::process(FDWriter* writer) {
  VLOG(4) << "io::FDWriter::CloseOp: begin";

  auto fdpair = writer->fd_->acquire_fd();
  ::fdatasync(fdpair.first);
  fdpair.second.unlock();

  auto r = writer->fd_->close();

  VLOG(4) << "io::FDWriter::CloseOp: end: "
          << "r=" << r;
  task->finish(std::move(r));
  return true;
}

class BufferedWriter : public WriterImpl {
 public:
  BufferedWriter(Writer w, PoolPtr p, std::size_t max_buffers) noexcept
      : w_(std::move(w)),
        chain_(std::move(p), max_buffers),
        closed_(false) {
    chain_.set_wrfn(
        [this](const base::Options& opts) { drain_callback(opts); });
  }

  BufferedWriter(Writer w, PoolPtr p) noexcept : w_(std::move(w)),
                                                 chain_(std::move(p)),
                                                 closed_(false) {
    chain_.set_wrfn(
        [this](const base::Options& opts) { drain_callback(opts); });
  }

  BufferedWriter(Writer w, std::size_t buffer_size, std::size_t max_buffers)
      : w_(std::move(w)), chain_(buffer_size, max_buffers), closed_(false) {
    chain_.set_wrfn(
        [this](const base::Options& opts) { drain_callback(opts); });
  }

  BufferedWriter(Writer w) : w_(std::move(w)), chain_(), closed_(false) {
    chain_.set_wrfn(
        [this](const base::Options& opts) { drain_callback(opts); });
  }

  std::size_t ideal_block_size() const noexcept override {
    return chain_.pool()->buffer_size();
  }

  bool is_buffered() const noexcept override { return true; }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
    chain_.write(task, n, ptr, len, opts);
  }

  void flush(event::Task* task, const base::Options& opts) override {
    CHECK_NOTNULL(task);
    if (!task->start()) return;
    auto* h = new DrainHelper(this, task, true, false, false, opts);
    h->next();
  }

  void sync(event::Task* task, const base::Options& opts) override {
    CHECK_NOTNULL(task);
    if (!task->start()) return;
    auto* h = new DrainHelper(this, task, true, true, false, opts);
    h->next();
  }

  void close(event::Task* task, const base::Options& opts) override {
    CHECK_NOTNULL(task);
    if (!task->start()) return;

    auto lock = base::acquire_lock(mu_);
    auto w = writer_closed();
    if (closed_) {
      task->finish(std::move(w));
      return;
    }
    chain_.fail_writes(w);
    chain_.fail_reads(w);
    chain_.flush();
    chain_.process();
    closed_ = true;
    lock.unlock();

    auto* h = new DrainHelper(this, task, true, true, true, opts);
    h->next();
  }

 private:
  struct DrainHelper {
    event::Task subtask;
    BufferedWriter* const self;
    event::Task* const /*nullable*/ task;
    const bool repeat;
    const bool sync;
    const bool close;
    const base::Options options;
    OwnedBuffer buffer;
    std::size_t n;
    base::Result write_result;
    base::Result sync_result;

    explicit DrainHelper(BufferedWriter* s, event::Task* /*nullable*/ t, bool r,
                         bool y, bool c, base::Options opts) noexcept
        : self(s),
          task(t),
          repeat(r),
          sync(y),
          close(c),
          options(opts),
          buffer(self->chain_.pool()->take()),
          n(0) {}

    void next() {
      std::size_t len = self->chain_.optimal_drain();
      std::size_t xlen = 0;
      self->chain_.drain(&xlen, buffer.data(), len);
      if (task) task->add_subtask(&subtask);
      self->w_.write(&subtask, &n, buffer.data(), xlen, options);
      subtask.on_finished(event::callback([this] {
        write_complete();
        return base::Result();
      }));
    }

    void do_sync() {
      if (task) task->add_subtask(&subtask);
      self->w_.sync(&subtask, options);
      subtask.on_finished(event::callback([this] {
        sync_complete();
        return base::Result();
      }));
    }

    void do_close() {
      if (task) task->add_subtask(&subtask);
      self->w_.close(&subtask, options);
      subtask.on_finished(event::callback([this] {
        close_complete();
        return base::Result();
      }));
    }

    void write_complete() {
      auto destroy = base::cleanup([this] { delete this; });

      base::Result r;
      if (subtask.result_will_throw()) {
        r = base::Result::unknown();
      } else {
        r = subtask.result();
      }

      bool again = repeat && r && self->chain_.optimal_drain() > 0;
      if (again) {
        subtask.reset();
        next();
        destroy.cancel();
        return;
      }

      if (!r) self->chain_.fail_writes(r);
      self->chain_.pool()->give(std::move(buffer));
      self->chain_.process();

      if (sync) {
        write_result = std::move(r);
        subtask.reset();
        do_sync();
        destroy.cancel();
        return;
      }

      if (close) {
        write_result = std::move(r);
        subtask.reset();
        do_close();
        destroy.cancel();
        return;
      }

      if (task) task->finish(std::move(r));
    }

    void sync_complete() {
      auto destroy = base::cleanup([this] { delete this; });

      base::Result r;
      if (subtask.result_will_throw()) {
        r = base::Result::unknown();
      } else {
        r = subtask.result();
      }

      if (close) {
        sync_result = std::move(r);
        subtask.reset();
        do_close();
        destroy.cancel();
        return;
      }

      r = write_result.and_then(r);
      if (task) task->finish(std::move(r));
    }

    void close_complete() {
      auto destroy = base::cleanup([this] { delete this; });

      base::Result r;
      if (subtask.result_will_throw()) {
        r = base::Result::unknown();
      } else {
        r = subtask.result();
      }

      r = r.and_then(write_result).and_then(sync_result);
      if (task) task->finish(std::move(r));
    }
  };

  void drain_callback(const base::Options& opts) {
    auto* h = new DrainHelper(this, nullptr, false, false, false, opts);
    h->next();
  }

  const Writer w_;
  Chain chain_;
  mutable std::mutex mu_;
  bool closed_;
};
}  // inline namespace implementation

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

template <typename... Args>
static Writer make_bufferedwriter(Args&&... args) {
  return Writer(std::make_shared<BufferedWriter>(std::forward<Args>(args)...));
}

Writer bufferedwriter(Writer w, PoolPtr pool, std::size_t max_buffers) {
  return make_bufferedwriter(std::move(w), std::move(pool), max_buffers);
}

Writer bufferedwriter(Writer w, PoolPtr pool) {
  return make_bufferedwriter(std::move(w), std::move(pool));
}

Writer bufferedwriter(Writer w, std::size_t buffer_size,
                      std::size_t max_buffers) {
  return make_bufferedwriter(std::move(w), buffer_size, max_buffers);
}

Writer bufferedwriter(Writer w) { return make_bufferedwriter(std::move(w)); }

base::Result writer_closed() {
  return base::Result::from_errno(EBADF, "io::Writer is closed");
}

base::Result writer_full() {
  return base::Result::from_errno(ENOSPC, "io::Writer is full");
}

}  // namespace io
