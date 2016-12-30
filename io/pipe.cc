#include "io/pipe.h"

#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>

#include "base/cleanup.h"
#include "base/logging.h"

static constexpr std::size_t kPipeIdealBlockSize = 1U << 20;  // 1 MiB

static base::Result closed_pipe() {
  return base::Result::failed_precondition("io::Pipe is closed");
}

namespace io {

namespace {
struct PipeGuts {
  struct ReadOp {
    event::Task* const task;
    char* const out;
    std::size_t* const n;
    const std::size_t min;
    const std::size_t max;
    const Options options;

    ReadOp(event::Task* t, char* o, std::size_t* n, std::size_t mn,
           std::size_t mx, Options opts) noexcept : task(t),
                                                    out(o),
                                                    n(n),
                                                    min(mn),
                                                    max(mx),
                                                    options(std::move(opts)) {}

    bool process(base::Lock& lock, PipeGuts* guts);
  };

  mutable std::mutex mu;
  bool write_closed;
  bool read_closed;
  std::vector<char> buffer;
  std::deque<std::unique_ptr<ReadOp>> queue;

  PipeGuts() noexcept : write_closed(false), read_closed(false) {}

  void process(base::Lock& lock) {
    while (!queue.empty()) {
      auto op = std::move(queue.front());
      queue.pop_front();
      if (!op->process(lock, this)) {
        queue.push_front(std::move(op));
        break;
      }
    }
  }
};

bool PipeGuts::ReadOp::process(base::Lock& lock, PipeGuts* guts) {
  auto& buf = guts->buffer;
  if (buf.size() >= min) {
    std::size_t count = buf.size();
    if (count > max) count = max;
    ::memcpy(out, buf.data(), count);
    *n = count;
    auto it = buf.begin();
    buf.erase(it, it + count);
    lock.unlock();
    auto cleanup = base::cleanup([&lock] { lock.lock(); });
    task->finish_ok();
    return true;
  }
  if (guts->write_closed) {
    ::memcpy(out, buf.data(), buf.size());
    *n = buf.size();
    buf.clear();
    lock.unlock();
    auto cleanup = base::cleanup([&lock] { lock.lock(); });
    task->finish(base::Result::eof());
    return true;
  }
  return false;
}

class PipeReader : public ReaderImpl {
 public:
  explicit PipeReader(std::shared_ptr<PipeGuts> g) noexcept
      : guts_(std::move(g)) {}

  ~PipeReader() noexcept {
    auto lock = base::acquire_lock(guts_->mu);
    close_guts(lock);
  }

  std::size_t ideal_block_size() const noexcept override {
    return kPipeIdealBlockSize;
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const Options& opts) override {
    if (!prologue(task, out, n, min, max)) return;
    auto lock = base::acquire_lock(guts_->mu);
    if (guts_->read_closed) {
      task->finish(closed_pipe());
      return;
    }
    guts_->queue.emplace_back(
        new PipeGuts::ReadOp(task, out, n, min, max, opts));
    guts_->process(lock);
  }

  void close(event::Task* task, const Options& opts) override {
    if (!prologue(task)) return;
    base::Result r;
    auto lock = base::acquire_lock(guts_->mu);
    if (guts_->read_closed) {
      r = closed_pipe();
      lock.unlock();
    } else {
      close_guts(lock);
    }
    task->finish(std::move(r));
  }

 private:
  void close_guts(base::Lock& lock) noexcept {
    guts_->write_closed = true;
    guts_->read_closed = true;
    auto q = std::move(guts_->queue);
    guts_->buffer.clear();
    guts_->queue.clear();
    lock.unlock();
    for (const auto& op : q) {
      op->task->finish_cancel();  // TODO: more appropriate error?
    }
  }

  std::shared_ptr<PipeGuts> guts_;
};

class PipeWriter : public WriterImpl {
 public:
  explicit PipeWriter(std::shared_ptr<PipeGuts> g) noexcept
      : guts_(std::move(g)) {}

  ~PipeWriter() noexcept {
    auto lock = base::acquire_lock(guts_->mu);
    guts_->write_closed = true;
    guts_->process(lock);
  }

  std::size_t ideal_block_size() const noexcept override {
    return kPipeIdealBlockSize;
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const Options& opts) override {
    if (!prologue(task, n, ptr, len)) return;
    base::Result r;
    auto lock = base::acquire_lock(guts_->mu);
    if (guts_->write_closed) {
      r = closed_pipe();
    } else {
      auto& buf = guts_->buffer;
      buf.insert(buf.end(), ptr, ptr + len);
      *n = len;
      guts_->process(lock);
    }
    lock.unlock();
    task->finish(std::move(r));
  }

  void close(event::Task* task, const Options& opts) override {
    base::Result r;
    auto lock = base::acquire_lock(guts_->mu);
    if (guts_->write_closed) {
      r = closed_pipe();
    } else {
      guts_->write_closed = true;
      guts_->process(lock);
    }
    lock.unlock();
    if (prologue(task)) task->finish(std::move(r));
  }

 private:
  std::shared_ptr<PipeGuts> guts_;
};
}  // anonymous namespace

void make_pipe(Reader* r, Writer* w) {
  CHECK_NOTNULL(r);
  CHECK_NOTNULL(w);
  auto guts = std::make_shared<PipeGuts>();
  *r = Reader(std::make_shared<PipeReader>(guts));
  *w = Writer(std::make_shared<PipeWriter>(std::move(guts)));
}

}  // namespace io
