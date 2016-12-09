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
  bool closed;
  std::vector<char> buffer;
  std::deque<ReadOp> readq;

  PipeGuts() noexcept : closed(false) {}

  bool process_one(const ReadOp& op) {
    if (closed) {
      op.task->finish(base::Result::failed_precondition("pipe is closed"));
      return true;
    }
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
    return false;
  }

  void process() {
    while (!readq.empty()) {
      const auto& op = readq.front();
      if (!process_one(op)) break;
      readq.pop_front();
    }
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) {
    auto lock = base::acquire_lock(mu);
    if (closed) {
      task->finish(base::Result::failed_precondition("pipe is closed"));
      return;
    }
    readq.emplace_back(task, out, n, min, max);
    process();
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len) {
    auto lock = base::acquire_lock(mu);
    if (closed) {
      task->finish(base::Result::failed_precondition("pipe is closed"));
      return;
    }
    buffer.insert(buffer.end(), ptr, ptr + len);
    *n = len;
    task->finish_ok();
    process();
  }

  void close(event::Task* task) {
    auto lock = base::acquire_lock(mu);
    if (closed) {
      task->finish(base::Result::failed_precondition("pipe is closed"));
      return;
    }
    closed = true;
    buffer.clear();
    readq.clear();
    task->finish_ok();
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
    guts_->read(task, out, n, min, max);
  }

  void close(event::Task* task) override {
    if (!prologue(task)) return;
    guts_->close(task);
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
    guts_->write(task, n, ptr, len);
  }

  void close(event::Task* task) override {
    if (!prologue(task)) return;
    guts_->close(task);
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
