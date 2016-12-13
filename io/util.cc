// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/util.h"

#include <type_traits>
#include <vector>

#include "base/logging.h"

namespace io {

namespace {
struct CopyHelper {
  event::Task* const task;
  std::size_t* const copied;
  const std::size_t max;
  const Writer writer;
  const Reader reader;
  event::Task subtask;
  BufferPool pool;
  OwnedBuffer buffer;
  std::size_t n;
  bool eof;

  CopyHelper(event::Task* t, std::size_t* c, std::size_t x, Writer w,
             Reader r) noexcept : task(t),
                                  copied(c),
                                  max(x),
                                  writer(std::move(w)),
                                  reader(std::move(r)),
                                  pool(choose_pool(writer, reader)),
                                  buffer(pool.take()),
                                  n(0),
                                  eof(false) {
    VLOG(6) << "io::CopyHelper::CopyHelper: max=" << max;
  }

  ~CopyHelper() noexcept { VLOG(6) << "io::CopyHelper::~CopyHelper"; }

  static std::size_t compute_block_size(const Writer& w, const Reader& r) {
    std::size_t wbsz = w.block_size();
    std::size_t rbsz = r.block_size();
    return (wbsz > rbsz) ? wbsz : rbsz;
  }

  static BufferPool choose_pool(const Writer& w, const Reader& r) {
    BufferPool wpool = w.options().pool();
    BufferPool rpool = r.options().pool();
    auto block_size = compute_block_size(w, r);
    if (wpool.buffer_size() < rpool.buffer_size()) {
      wpool.swap(rpool);
    }
    if (wpool.buffer_size() < block_size) {
      wpool = BufferPool(block_size, null_pool);
    }
    return wpool;
  }

  void begin() {
    VLOG(6) << "io::CopyHelper::begin";
    task->add_subtask(&subtask);
    reader.write_to(&subtask, copied, max, writer);
    auto closure = [this] { return write_to_complete(); };
    subtask.on_finished(event::callback(closure));
  }

  base::Result write_to_complete() {
    auto r = subtask.result();
    VLOG(6) << "io::CopyHelper::write_to_complete, r=" << r
            << ", copied=" << *copied;
    if (r.code() != base::Result::Code::NOT_IMPLEMENTED) {
      task->finish(std::move(r));
      delete this;
      return base::Result();
    }
    subtask.reset();
    task->add_subtask(&subtask);
    writer.read_from(&subtask, copied, max, reader);
    auto closure = [this] { return read_from_complete(); };
    subtask.on_finished(event::callback(closure));
    return base::Result();
  }

  base::Result read_from_complete() {
    auto r = subtask.result();
    VLOG(6) << "io::CopyHelper::read_from_complete: r=" << r
            << ", copied=" << *copied;
    if (r.code() != base::Result::Code::NOT_IMPLEMENTED) {
      task->finish(std::move(r));
      delete this;
      return base::Result();
    }
    subtask.reset();
    task->add_subtask(&subtask);
    std::size_t min = 1;
    std::size_t len = buffer.size();
    if (len > max - *copied) len = max - *copied;
    if (min > len) min = len;
    reader.read(&subtask, buffer.data(), &n, min, len);
    auto closure = [this] { return fallback_read_complete(); };
    subtask.on_finished(event::callback(closure));
    return base::Result();
  }

  base::Result fallback_read_complete() {
    auto r = subtask.result();
    VLOG(6) << "io::CopyHelper::fallback_read_complete: r=" << r
            << ", *copied=" << *copied << ", n=" << n;
    switch (r.code()) {
      case base::Result::Code::OK:
        eof = (n == 0);
        break;

      case base::Result::Code::END_OF_FILE:
        eof = true;
        break;

      default:
        task->finish(std::move(r));
        delete this;
        return base::Result();
    }
    subtask.reset();
    task->add_subtask(&subtask);
    const char* ptr = buffer.data();
    std::size_t len = n;
    n = 0;
    writer.write(&subtask, &n, ptr, len);
    auto closure = [this] { return fallback_write_complete(); };
    subtask.on_finished(event::callback(closure));
    return base::Result();
  }

  base::Result fallback_write_complete() {
    auto r = subtask.result();
    *copied += n;
    VLOG(6) << "io::CopyHelper::fallback_write_complete: r=" << r
            << ", *copied=" << *copied << ", eof=" << std::boolalpha << eof;
    if (eof || !r.ok()) {
      task->finish(std::move(r));
      delete this;
      return base::Result();
    }
    subtask.reset();
    task->add_subtask(&subtask);
    std::size_t min = 1;
    std::size_t len = buffer.size();
    if (len > max - *copied) len = max - *copied;
    if (min > len) min = len;
    reader.read(&subtask, buffer.data(), &n, min, len);
    auto closure = [this] { return fallback_read_complete(); };
    subtask.on_finished(event::callback(closure));
    return base::Result();
  }
};
}  // anonymous namespace

void copy_n(event::Task* task, std::size_t* copied, std::size_t max, Writer w,
            Reader r) {
  *copied = 0;
  if (!task->start()) return;
  auto* helper = new CopyHelper(task, copied, max, std::move(w), std::move(r));
  helper->begin();
}

}  // namespace io
