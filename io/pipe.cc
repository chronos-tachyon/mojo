#include "io/pipe.h"

#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>

#include "base/logging.h"

namespace io {

namespace {
struct ReadOp {
  event::Task* task;
  char* out;
  std::size_t* n;
  std::size_t min;
  std::size_t max;

  ReadOp(event::Task* t, char* o, std::size_t* n, std::size_t mn,
         std::size_t mx) noexcept : task(t),
                                    out(o),
                                    n(n),
                                    min(mn),
                                    max(mx) {}
};

struct PipeGuts {
  mutable std::mutex mu;
  bool write_closed;
  bool read_closed;
  std::vector<char> buffer;
  std::deque<ReadOp> readq;

  PipeGuts() noexcept : write_closed(false), read_closed(false) {}

  bool process_one(const ReadOp& op) {
    if (buffer.size() >= op.min) {
      std::size_t count = buffer.size();
      if (count > op.max) count = op.max;
      ::memcpy(op.out, buffer.data(), count);
      *op.n = count;
      auto it = buffer.begin();
      buffer.erase(it, it + count);
      op.task->finish_ok();
      return true;
    }
    if (write_closed) {
      ::memcpy(op.out, buffer.data(), buffer.size());
      *op.n = buffer.size();
      buffer.clear();
      op.task->finish(base::Result::eof());
      return true;
    }
    return false;
  }

  void process() {
    while (!readq.empty()) {
      const auto& op = readq.front();
      if (!process_one(op)) break;
      readq.pop_front();
    }
  }
};

class PipeReader : public ReaderImpl {
 public:
  PipeReader(std::shared_ptr<PipeGuts> g, Options o) noexcept
      : ReaderImpl(std::move(o)),
        guts_(std::move(g)) {}

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) override {
    if (!prologue(task, out, n, min, max)) return;
    auto lock = base::acquire_lock(guts_->mu);
    if (guts_->read_closed) {
      task->finish(base::Result::failed_precondition("pipe is closed"));
      return;
    }
    guts_->readq.emplace_back(task, out, n, min, max);
    guts_->process();
  }

  void close(event::Task* task) override {
    if (!prologue(task)) return;
    auto lock = base::acquire_lock(guts_->mu);
    if (guts_->read_closed) {
      task->finish(base::Result::failed_precondition("pipe is closed"));
      return;
    }
    guts_->write_closed = true;
    guts_->read_closed = true;
    for (const auto& op : guts_->readq) {
      op.task->finish_cancel();
    }
    guts_->buffer.clear();
    guts_->readq.clear();
    task->finish_ok();
  }

 private:
  std::shared_ptr<PipeGuts> guts_;
};

class PipeWriter : public WriterImpl {
 public:
  PipeWriter(std::shared_ptr<PipeGuts> g, Options o) noexcept
      : WriterImpl(std::move(o)),
        guts_(std::move(g)) {}

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len) override {
    if (!prologue(task, n, ptr, len)) return;
    auto lock = base::acquire_lock(guts_->mu);
    if (guts_->write_closed) {
      task->finish(base::Result::failed_precondition("pipe is closed"));
      return;
    }
    auto& buf = guts_->buffer;
    buf.insert(buf.end(), ptr, ptr + len);
    *n = len;
    task->finish_ok();
    guts_->process();
  }

  void close(event::Task* task) override {
    if (!prologue(task)) return;
    auto lock = base::acquire_lock(guts_->mu);
    if (guts_->write_closed) {
      task->finish(base::Result::failed_precondition("pipe is closed"));
      return;
    }
    guts_->write_closed = true;
    task->finish_ok();
    guts_->process();
  }

 private:
  std::shared_ptr<PipeGuts> guts_;
};
}  // anonymous namespace

void make_pipe(Reader* r, Writer* w, Options o) {
  CHECK_NOTNULL(r);
  CHECK_NOTNULL(w);
  auto guts = std::make_shared<PipeGuts>();
  *r = Reader(std::make_shared<PipeReader>(guts, o));
  *w = Writer(std::make_shared<PipeWriter>(std::move(guts), std::move(o)));
}

}  // namespace io
