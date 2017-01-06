// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/util.h"

#include <type_traits>
#include <vector>

#include "base/logging.h"

__attribute__((const)) static std::size_t gcd(std::size_t a,
                                              std::size_t b) noexcept {
  std::size_t t;
  while (b != 0) {
    t = b;
    b = a % b;
    a = t;
  }
  return a;
}

__attribute__((const)) static std::size_t lcm(std::size_t a,
                                              std::size_t b) noexcept {
  return a / gcd(a, b) * b;
}

namespace io {

namespace {
struct CopyHelper {
  event::Task* const task;
  std::size_t* const copied;
  const std::size_t max;
  const Writer writer;
  const Reader reader;
  const base::Options options;
  const std::size_t block_size;
  BufferPool pool;
  OwnedBuffer buffer;
  event::Task subtask;
  std::size_t n;
  bool eof;

  CopyHelper(event::Task* t, std::size_t* c, std::size_t x, Writer w, Reader r,
             base::Options opts) noexcept
      : task(t),
        copied(c),
        max(x),
        writer(std::move(w)),
        reader(std::move(r)),
        options(std::move(opts)),
        block_size(compute_block_size(writer, reader, options)),
        pool(choose_pool(block_size, options)),
        buffer(pool.take()),
        n(0),
        eof(false) {
    VLOG(6) << "io::CopyHelper::CopyHelper: max=" << max;
  }

  ~CopyHelper() noexcept { VLOG(6) << "io::CopyHelper::~CopyHelper"; }

  static std::size_t compute_block_size(const Writer& w, const Reader& r,
                                        const base::Options& o) {
    std::size_t blksz = o.get<io::Options>().block_size;
    if (blksz != 0) return blksz;
    std::size_t wblksz = w.ideal_block_size();
    std::size_t rblksz = r.ideal_block_size();
    return lcm(wblksz, rblksz);
  }

  static BufferPool choose_pool(std::size_t block_size,
                                const base::Options& o) {
    BufferPool pool = o.get<io::Options>().pool;
    if (pool.buffer_size() >= block_size)
      return pool;
    else
      return BufferPool(block_size, null_pool);
  }

  void begin() {
    VLOG(6) << "io::CopyHelper::begin";
    task->add_subtask(&subtask);
    reader.write_to(&subtask, &n, max, writer, options);
    auto closure = [this] { return write_to_complete(); };
    subtask.on_finished(event::callback(closure));
  }

  base::Result write_to_complete() {
    *copied += n;
    auto r = subtask.result();
    VLOG(6) << "io::CopyHelper::write_to_complete: "
            << "*copied=" << *copied << ", "
            << "n=" << n << ", "
            << "r=" << r;
    if (r.code() != base::ResultCode::NOT_IMPLEMENTED) {
      task->finish(std::move(r));
      delete this;
      return base::Result();
    }
    subtask.reset();
    task->add_subtask(&subtask);
    writer.read_from(&subtask, &n, max, reader, options);
    auto closure = [this] { return read_from_complete(); };
    subtask.on_finished(event::callback(closure));
    return base::Result();
  }

  base::Result read_from_complete() {
    *copied += n;
    auto r = subtask.result();
    VLOG(6) << "io::CopyHelper::read_from_complete: "
            << "*copied=" << *copied << ", "
            << "n=" << n << ", "
            << "r=" << r;
    if (r.code() != base::ResultCode::NOT_IMPLEMENTED) {
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
    reader.read(&subtask, buffer.data(), &n, min, len, options);
    auto closure = [this] { return fallback_read_complete(); };
    subtask.on_finished(event::callback(closure));
    return base::Result();
  }

  base::Result fallback_read_complete() {
    auto r = subtask.result();
    VLOG(6) << "io::CopyHelper::fallback_read_complete: "
            << "*copied=" << *copied << ", "
            << "n=" << n << ", "
            << "r=" << r;
    switch (r.code()) {
      case base::ResultCode::OK:
        eof = (n == 0);
        break;

      case base::ResultCode::END_OF_FILE:
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
    writer.write(&subtask, &n, ptr, len, options);
    auto closure = [this] { return fallback_write_complete(); };
    subtask.on_finished(event::callback(closure));
    return base::Result();
  }

  base::Result fallback_write_complete() {
    *copied += n;
    auto r = subtask.result();
    VLOG(6) << "io::CopyHelper::fallback_write_complete: "
            << "*copied=" << *copied << ", "
            << "n=" << n << ", "
            << "eof=" << std::boolalpha << eof << ", "
            << "r=" << r;
    if (eof || !r) {
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
    reader.read(&subtask, buffer.data(), &n, min, len, options);
    auto closure = [this] { return fallback_read_complete(); };
    subtask.on_finished(event::callback(closure));
    return base::Result();
  }
};
}  // anonymous namespace

void copy_n(event::Task* task, std::size_t* copied, std::size_t max, Writer w,
            Reader r, const base::Options& opts) {
  *copied = 0;
  if (!task->start()) return;
  auto* helper =
      new CopyHelper(task, copied, max, std::move(w), std::move(r), opts);
  helper->begin();
}

}  // namespace io
