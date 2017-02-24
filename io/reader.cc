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

#include <condition_variable>
#include <cstring>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>

#include "base/backport.h"
#include "base/cleanup.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "io/buffer.h"
#include "io/chain.h"
#include "io/writer.h"

namespace io {

using RC = base::ResultCode;

static constexpr std::size_t kSendfileMax = 4U << 20;  // 4 MiB
static constexpr std::size_t kSpliceMax = 4U << 20;    // 4 MiB

static constexpr bool s16_holds_smallest() {
  return std::numeric_limits<int16_t>::min() < -0x7fff;
}

static constexpr bool s32_holds_smallest() {
  return std::numeric_limits<int32_t>::min() < -0x7fffffff;
}

static constexpr bool s64_holds_smallest() {
  return std::numeric_limits<int64_t>::min() < -0x7fffffffffffffffLL;
}

static TransferMode default_transfer_mode() noexcept {
  return TransferMode::read_write;  // TODO: probe this on first access
}

bool ReaderImpl::prologue(event::Task* task, char* out, std::size_t* n,
                          std::size_t min, std::size_t max) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(n);
  CHECK_GE(min, 0U);
  CHECK_LE(min, max);
  if (max > 0) CHECK_NOTNULL(out);

  bool start = task->start();
  if (start) *n = 0;
  return start;
}

bool ReaderImpl::prologue(event::Task* task, std::size_t* n, std::size_t max,
                          const Writer& w) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(n);
  CHECK_GE(max, 0U);
  w.assert_valid();

  bool start = task->start();
  if (start) *n = 0;
  return start;
}

bool ReaderImpl::prologue(event::Task* task) {
  CHECK_NOTNULL(task);
  return task->start();
}

void ReaderImpl::write_to(event::Task* task, std::size_t* n, std::size_t max,
                          const Writer& w, const base::Options& opts) {
  if (prologue(task, n, max, w)) task->finish(base::Result::not_implemented());
}

void Reader::assert_valid() const { CHECK(ptr_) << ": io::Reader is empty!"; }

inline namespace implementation {
struct StringReadHelper {
  event::Task* const task;
  std::string* const out;
  event::Task subtask;
  PoolPtr pool;
  OwnedBuffer buffer;
  std::size_t n;
  bool give_back;

  StringReadHelper(event::Task* t, std::string* o, PoolPtr p, OwnedBuffer b,
                   bool g)
      : task(t),
        out(o),
        pool(std::move(p)),
        buffer(std::move(b)),
        n(0),
        give_back(g) {}

  base::Result run() {
    out->append(buffer.data(), n);
    if (give_back) pool->give(std::move(buffer));
    event::propagate_result(task, &subtask);
    delete this;
    return base::Result();
  }
};
}  // inline namespace implementation

void Reader::read(event::Task* task, std::string* out, std::size_t min,
                  std::size_t max, const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  out->clear();

  PoolPtr pool = opts.get<io::Options>().pool;
  OwnedBuffer buf;
  bool give_back;
  if (pool && pool->size() >= max) {
    buf = pool->take();
    give_back = true;
  } else {
    buf = OwnedBuffer(max);
    give_back = false;
  }

  auto* helper = new StringReadHelper(task, out, std::move(pool),
                                      std::move(buf), give_back);
  task->add_subtask(&helper->subtask);
  read(&helper->subtask, helper->buffer.data(), &helper->n, min, max, opts);
  auto closure = [helper] { return helper->run(); };
  helper->subtask.on_finished(event::callback(closure));
}

void Reader::read_u8(event::Task* task, uint8_t* out,
                     const base::Options& opts) const {
  struct Helper : public event::Callback {
    event::Task subtask;
    event::Task* const task;
    uint8_t* out;
    char lone;
    std::size_t n;

    Helper(event::Task* t, uint8_t* o) noexcept : task(t),
                                                  out(o),
                                                  lone(0),
                                                  n(0) {}

    base::Result run() override {
      // TODO: consider adding ReaderImpl::unread() [default not_implemented] to
      // avoid data loss in the error result case
      *out = *reinterpret_cast<const unsigned char*>(&lone);
      event::propagate_result(task, &subtask);
      return base::Result();
    }
  };

  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = 0;

  auto helper = base::backport::make_unique<Helper>(task, out);
  auto* h = helper.get();
  task->add_subtask(&h->subtask);
  read(&h->subtask, &h->lone, &h->n, 1, 1, opts);
  h->subtask.on_finished(std::move(helper));
}

void Reader::read_u16(event::Task* task, uint16_t* out,
                      const base::Endian* endian,
                      const base::Options& opts) const {
  struct Helper : public event::Callback {
    event::Task subtask;
    event::Task* const task;
    uint16_t* const out;
    const base::Endian* const endian;
    char buf[2];
    std::size_t n;

    Helper(event::Task* t, uint16_t* o, const base::Endian* e) noexcept
        : task(t),
          out(o),
          endian(e),
          n(0) {}

    base::Result run() override {
      // TODO: consider adding ReaderImpl::unread() [default not_implemented] to
      // avoid data loss in the error result case
      if (event::propagate_failure(task, &subtask)) return base::Result();
      *out = endian->get_u16(buf);
      task->finish_ok();
      return base::Result();
    }
  };

  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(endian);
  if (!task->start()) return;
  *out = 0;

  auto helper = base::backport::make_unique<Helper>(task, out, endian);
  auto* h = helper.get();
  task->add_subtask(&h->subtask);
  read(&h->subtask, h->buf, &h->n, 2, 2, opts);
  h->subtask.on_finished(std::move(helper));
}

void Reader::read_u32(event::Task* task, uint32_t* out,
                      const base::Endian* endian,
                      const base::Options& opts) const {
  struct Helper : public event::Callback {
    event::Task subtask;
    event::Task* const task;
    uint32_t* const out;
    const base::Endian* const endian;
    char buf[4];
    std::size_t n;

    Helper(event::Task* t, uint32_t* o, const base::Endian* e) noexcept
        : task(t),
          out(o),
          endian(e),
          n(0) {}

    base::Result run() override {
      // TODO: consider adding ReaderImpl::unread() [default not_implemented] to
      // avoid data loss in the error result case
      if (event::propagate_failure(task, &subtask)) return base::Result();
      *out = endian->get_u32(buf);
      task->finish_ok();
      return base::Result();
    }
  };

  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(endian);
  if (!task->start()) return;
  *out = 0;

  auto helper = base::backport::make_unique<Helper>(task, out, endian);
  auto* h = helper.get();
  task->add_subtask(&h->subtask);
  read(&h->subtask, h->buf, &h->n, 4, 4, opts);
  h->subtask.on_finished(std::move(helper));
}

void Reader::read_u64(event::Task* task, uint64_t* out,
                      const base::Endian* endian,
                      const base::Options& opts) const {
  struct Helper : public event::Callback {
    event::Task subtask;
    event::Task* const task;
    uint64_t* const out;
    const base::Endian* const endian;
    char buf[8];
    std::size_t n;

    Helper(event::Task* t, uint64_t* o, const base::Endian* e) noexcept
        : task(t),
          out(o),
          endian(e),
          n(0) {}

    base::Result run() override {
      // TODO: consider adding ReaderImpl::unread() [default not_implemented] to
      // avoid data loss in the error result case
      if (event::propagate_failure(task, &subtask)) return base::Result();
      *out = endian->get_u64(buf);
      task->finish_ok();
      return base::Result();
    }
  };

  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(endian);
  if (!task->start()) return;
  *out = 0;

  auto helper = base::backport::make_unique<Helper>(task, out, endian);
  auto* h = helper.get();
  task->add_subtask(&h->subtask);
  read(&h->subtask, h->buf, &h->n, 8, 8, opts);
  h->subtask.on_finished(std::move(helper));
}

void Reader::read_s8(event::Task* task, int8_t* out,
                     const base::Options& opts) const {
  struct Helper : public event::Callback {
    event::Task subtask;
    event::Task* const task;
    int8_t* out;
    char lone;
    std::size_t n;

    Helper(event::Task* t, int8_t* o) noexcept : task(t),
                                                 out(o),
                                                 lone(0),
                                                 n(0) {}

    base::Result run() override {
      // TODO: consider adding ReaderImpl::unread() [default not_implemented] to
      // avoid data loss in the error result case
      uint8_t tmp = *reinterpret_cast<const unsigned char*>(&lone);
      static constexpr uint8_t K = uint8_t(1U) << 7;
      if (tmp < K)
        *out = tmp;
      else
        *out = -int8_t(~tmp) - 1;
      event::propagate_result(task, &subtask);
      return base::Result();
    }
  };

  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = 0;

  auto helper = base::backport::make_unique<Helper>(task, out);
  auto* h = helper.get();
  task->add_subtask(&h->subtask);
  read(&h->subtask, &h->lone, &h->n, 1, 1, opts);
  h->subtask.on_finished(std::move(helper));
}

void Reader::read_s16(event::Task* task, int16_t* out,
                      const base::Endian* endian,
                      const base::Options& opts) const {
  struct Helper : public event::Callback {
    event::Task subtask;
    event::Task* const task;
    int16_t* const out;
    const base::Endian* const endian;
    char buf[2];
    std::size_t n;

    Helper(event::Task* t, int16_t* o, const base::Endian* e) noexcept
        : task(t),
          out(o),
          endian(e),
          n(0) {}

    base::Result run() override {
      // TODO: consider adding ReaderImpl::unread() [default not_implemented] to
      // avoid data loss in the error result case
      if (event::propagate_failure(task, &subtask)) return base::Result();
      uint16_t tmp = endian->get_u16(buf);
      static constexpr uint16_t K = uint16_t(1U) << 15;
      if (tmp == K && !s16_holds_smallest()) {
        task->finish(base::Result::out_of_range(
            "int16_t cannot hold -2**15 on this platform"));
        return base::Result();
      }
      if (tmp < K)
        *out = tmp;
      else
        *out = -int16_t(~tmp) - 1;
      task->finish_ok();
      return base::Result();
    }
  };

  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(endian);
  if (!task->start()) return;
  *out = 0;

  auto helper = base::backport::make_unique<Helper>(task, out, endian);
  auto* h = helper.get();
  task->add_subtask(&h->subtask);
  read(&h->subtask, h->buf, &h->n, 2, 2, opts);
  h->subtask.on_finished(std::move(helper));
}

void Reader::read_s32(event::Task* task, int32_t* out,
                      const base::Endian* endian,
                      const base::Options& opts) const {
  struct Helper : public event::Callback {
    event::Task subtask;
    event::Task* const task;
    int32_t* const out;
    const base::Endian* const endian;
    char buf[4];
    std::size_t n;

    Helper(event::Task* t, int32_t* o, const base::Endian* e) noexcept
        : task(t),
          out(o),
          endian(e),
          n(0) {}

    base::Result run() override {
      // TODO: consider adding ReaderImpl::unread() [default not_implemented] to
      // avoid data loss in the error result case
      if (event::propagate_failure(task, &subtask)) return base::Result();
      uint32_t tmp = endian->get_u32(buf);
      static constexpr uint32_t K = uint32_t(1U) << 31;
      if (tmp == K && !s32_holds_smallest()) {
        task->finish(base::Result::out_of_range(
            "int32_t cannot hold -2**31 on this platform"));
        return base::Result();
      }
      if (tmp < K)
        *out = tmp;
      else
        *out = -int32_t(~tmp) - 1;
      task->finish_ok();
      return base::Result();
    }
  };

  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(endian);
  if (!task->start()) return;
  *out = 0;

  auto helper = base::backport::make_unique<Helper>(task, out, endian);
  auto* h = helper.get();
  task->add_subtask(&h->subtask);
  read(&h->subtask, h->buf, &h->n, 4, 4, opts);
  h->subtask.on_finished(std::move(helper));
}

void Reader::read_s64(event::Task* task, int64_t* out,
                      const base::Endian* endian,
                      const base::Options& opts) const {
  struct Helper : public event::Callback {
    event::Task subtask;
    event::Task* const task;
    int64_t* const out;
    const base::Endian* const endian;
    char buf[8];
    std::size_t n;

    Helper(event::Task* t, int64_t* o, const base::Endian* e) noexcept
        : task(t),
          out(o),
          endian(e),
          n(0) {}

    base::Result run() override {
      // TODO: consider adding ReaderImpl::unread() [default not_implemented] to
      // avoid data loss in the error result case
      if (event::propagate_failure(task, &subtask)) return base::Result();
      uint64_t tmp = endian->get_u64(buf);
      static constexpr uint64_t K = uint64_t(1U) << 63;
      if (tmp == K && !s64_holds_smallest()) {
        task->finish(base::Result::out_of_range(
            "int64_t cannot hold -2**63 on this platform"));
        return base::Result();
      }
      if (tmp < K)
        *out = tmp;
      else
        *out = -int64_t(~tmp) - 1;
      task->finish_ok();
      return base::Result();
    }
  };

  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(endian);
  if (!task->start()) return;
  *out = 0;

  auto helper = base::backport::make_unique<Helper>(task, out, endian);
  auto* h = helper.get();
  task->add_subtask(&h->subtask);
  read(&h->subtask, h->buf, &h->n, 8, 8, opts);
  h->subtask.on_finished(std::move(helper));
}

void Reader::read_uvarint(event::Task* task, uint64_t* out,
                          const base::Options& opts) const {
  struct Helper {
    event::Task subtask;
    const Reader reader;
    event::Task* const task;
    uint64_t* const out;
    const base::Options options;
    char buf[10];
    std::size_t n;
    std::size_t x;

    Helper(Reader r, event::Task* t, uint64_t* o, base::Options opts) noexcept
        : reader(std::move(r)),
          task(t),
          out(o),
          options(std::move(opts)),
          n(0),
          x(0) {
      ::bzero(buf, sizeof(buf));
    }

    void next() {
      task->add_subtask(&subtask);
      reader.read(&subtask, buf + x, &n, 1, 1, options);
      subtask.on_finished(event::callback([this] {
        read_complete();
        return base::Result();
      }));
    }

    void read_complete() {
      auto destroy = base::cleanup([this] { delete this; });

      // TODO: consider adding ReaderImpl::unread() [default not_implemented]
      // to avoid data loss in the error result case
      if (event::propagate_failure(task, &subtask)) return;

      const auto* p = reinterpret_cast<const unsigned char*>(&buf);
      if ((p[x] & 0x80U) == 0) {
        using W = uint64_t;
        *out = W(p[0] & 0x7fU) | (W(p[1] & 0x7fU) << 7) |
               (W(p[2] & 0x7fU) << 14) | (W(p[3] & 0x7fU) << 21) |
               (W(p[4] & 0x7fU) << 28) | (W(p[5] & 0x7fU) << 35) |
               (W(p[6] & 0x7fU) << 42) | (W(p[7] & 0x7fU) << 49) |
               (W(p[8] & 0x7fU) << 56) | (W(p[9] & 0x01U) << 63);
        task->finish_ok();
        return;
      }

      ++x;
      if (x >= sizeof(buf)) {
        task->finish(base::Result::data_loss("invalid varint byte sequence"));
        return;
      }

      destroy.cancel();
      subtask.reset();
      next();
    }
  };

  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = 0;

  auto* h = new Helper(*this, task, out, opts);
  h->next();
}

void Reader::read_svarint(event::Task* task, int64_t* out,
                          const base::Options& opts) const {
  struct Helper : public event::Callback {
    event::Task subtask;
    event::Task* const task;
    int64_t* const out;
    uint64_t tmp;

    Helper(event::Task* t, int64_t* o) noexcept : task(t), out(o) {}

    base::Result run() override {
      if (!event::propagate_failure(task, &subtask)) {
        static constexpr uint64_t K = uint64_t(1U) << 63;
        if (tmp == K && !s64_holds_smallest()) {
          task->finish(base::Result::out_of_range(
              "int64_t cannot hold -2**63 on this platform"));
        } else {
          if (tmp < K)
            *out = tmp;
          else
            *out = -int64_t(~tmp) - 1;
          task->finish_ok();
        }
      }
      return base::Result();
    }
  };

  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = 0;

  auto helper = base::backport::make_unique<Helper>(task, out);
  auto* h = helper.get();
  task->add_subtask(&h->subtask);
  read_uvarint(&h->subtask, &h->tmp, opts);
  h->subtask.on_finished(std::move(helper));
}

void Reader::read_svarint_zigzag(event::Task* task, int64_t* out,
                                 const base::Options& opts) const {
  struct Helper : public event::Callback {
    event::Task subtask;
    event::Task* const task;
    int64_t* const out;
    uint64_t tmp;

    Helper(event::Task* t, int64_t* o) noexcept : task(t), out(o) {}

    base::Result run() override {
      if (!event::propagate_failure(task, &subtask)) {
        static constexpr uint64_t K = 0xffffffffffffffffULL;
        if (tmp == K && !s64_holds_smallest()) {
          task->finish(base::Result::out_of_range(
              "int64_t cannot hold -2**63 on this platform"));
        } else {
          if (tmp & 1)
            *out = -int64_t(tmp >> 1) - 1;
          else
            *out = int64_t(tmp >> 1);
          task->finish_ok();
        }
      }
      return base::Result();
    }
  };

  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = 0;

  auto helper = base::backport::make_unique<Helper>(task, out);
  auto* h = helper.get();
  task->add_subtask(&h->subtask);
  read_uvarint(&h->subtask, &h->tmp, opts);
  h->subtask.on_finished(std::move(helper));
}

inline namespace implementation {
struct ReadLineHelper {
  event::Task subtask;
  Reader reader;
  event::Task* const task;
  std::string* const out;
  const std::size_t max;
  const base::Options options;
  char ch;
  std::size_t n;

  explicit ReadLineHelper(Reader r, event::Task* t, std::string* o,
                          std::size_t mx, base::Options opts) noexcept
      : reader(std::move(r)),
        task(t),
        out(o),
        max(mx),
        options(std::move(opts)) {}

  void next() {
    if (out->size() >= max) {
      task->finish_ok();
      delete this;
      return;
    }
    subtask.reset();
    task->add_subtask(&subtask);
    reader.read(&subtask, &ch, &n, 1, 1, options);
    subtask.on_finished(event::callback([this] {
      read_complete();
      return base::Result();
    }));
  }

  void read_complete() {
    if (event::propagate_failure(task, &subtask)) {
      delete this;
      return;
    }
    out->push_back(ch);
    if (ch == '\n') {
      task->finish_ok();
      delete this;
      return;
    }
    next();
  }
};
}  // inline namespace implementation

void Reader::readline(event::Task* task, std::string* out, std::size_t max,
                      const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  out->clear();

  auto* h = new ReadLineHelper(*this, task, out, max, opts);
  h->next();
}

base::Result Reader::read(char* out, std::size_t* n, std::size_t min,
                          std::size_t max, const base::Options& opts) const {
  event::Task task;
  read(&task, out, n, min, max, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read(std::string* out, std::size_t min, std::size_t max,
                          const base::Options& opts) const {
  event::Task task;
  read(&task, out, min, max, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read_u8(uint8_t* out, const base::Options& opts) const {
  event::Task task;
  read_u8(&task, out, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read_u16(uint16_t* out, const base::Endian* endian,
                              const base::Options& opts) const {
  event::Task task;
  read_u16(&task, out, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read_u32(uint32_t* out, const base::Endian* endian,
                              const base::Options& opts) const {
  event::Task task;
  read_u32(&task, out, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read_u64(uint64_t* out, const base::Endian* endian,
                              const base::Options& opts) const {
  event::Task task;
  read_u64(&task, out, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read_s8(int8_t* out, const base::Options& opts) const {
  event::Task task;
  read_s8(&task, out, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read_s16(int16_t* out, const base::Endian* endian,
                              const base::Options& opts) const {
  event::Task task;
  read_s16(&task, out, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read_s32(int32_t* out, const base::Endian* endian,
                              const base::Options& opts) const {
  event::Task task;
  read_s32(&task, out, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read_s64(int64_t* out, const base::Endian* endian,
                              const base::Options& opts) const {
  event::Task task;
  read_s64(&task, out, endian, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read_uvarint(uint64_t* out,
                                  const base::Options& opts) const {
  event::Task task;
  read_uvarint(&task, out, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read_svarint(int64_t* out,
                                  const base::Options& opts) const {
  event::Task task;
  read_svarint(&task, out, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::read_svarint_zigzag(int64_t* out,
                                         const base::Options& opts) const {
  event::Task task;
  read_svarint_zigzag(&task, out, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::readline(std::string* out, std::size_t max,
                              const base::Options& opts) const {
  event::Task task;
  readline(&task, out, max, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::write_to(std::size_t* n, std::size_t max, const Writer& w,
                              const base::Options& opts) const {
  event::Task task;
  write_to(&task, n, max, w, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

base::Result Reader::close(const base::Options& opts) const {
  event::Task task;
  close(&task, opts);
  event::wait(get_manager(opts), &task);
  return task.result();
}

inline namespace implementation {
class FunctionReader : public ReaderImpl {
 public:
  FunctionReader(ReadFn rfn, CloseFn cfn)
      : rfn_(std::move(rfn)), cfn_(std::move(cfn)) {}

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override {
    rfn_(task, out, n, min, max, opts);
  }

  void close(event::Task* task, const base::Options& opts) override {
    cfn_(task, opts);
  }

 private:
  ReadFn rfn_;
  CloseFn cfn_;
};

class SyncFunctionReader : public ReaderImpl {
 public:
  SyncFunctionReader(SyncReadFn rfn, SyncCloseFn cfn)
      : rfn_(std::move(rfn)), cfn_(std::move(cfn)) {}

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override {
    if (prologue(task, out, n, min, max))
      task->finish(rfn_(out, n, min, max, opts));
  }

  void close(event::Task* task, const base::Options& opts) override {
    if (prologue(task)) task->finish(cfn_(opts));
  }

 private:
  SyncReadFn rfn_;
  SyncCloseFn cfn_;
};

class CloseIgnoringReader : public ReaderImpl {
 public:
  CloseIgnoringReader(Reader r) : r_(std::move(r)) {}

  std::size_t ideal_block_size() const noexcept override {
    return r_.ideal_block_size();
  }

  bool is_buffered() const noexcept override { return r_.is_buffered(); }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override {
    r_.read(task, out, n, min, max, opts);
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w, const base::Options& opts) override {
    r_.write_to(task, n, max, w, opts);
  }

  void close(event::Task* task, const base::Options& opts) override {
    if (prologue(task)) task->finish_ok();
  }

 private:
  Reader r_;
};

class LimitedReader : public ReaderImpl {
 public:
  LimitedReader(Reader r, std::size_t max)
      : r_(std::move(r)), remaining_(max) {}

  ~LimitedReader() noexcept override { mu_.lock(); }

  std::size_t ideal_block_size() const noexcept override {
    return r_.ideal_block_size();
  }

  bool is_buffered() const noexcept override { return r_.is_buffered(); }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override {
    if (!prologue(task, out, n, min, max)) return;

    auto lock = base::acquire_lock(mu_);
    std::size_t amax = std::min(max, remaining_);
    std::size_t amin = std::min(min, remaining_);
    bool eof = (amax < min);

    auto helper = base::backport::make_unique<Helper>(task, n, &remaining_, eof,
                                                      std::move(lock));
    auto* st = &helper->subtask;
    r_.read(st, out, n, amin, amax, opts);
    st->on_finished(std::move(helper));
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w, const base::Options& opts) override {
    if (!prologue(task, n, max, w)) return;

    auto lock = base::acquire_lock(mu_);
    std::size_t amax = std::min(max, remaining_);

    auto helper = base::backport::make_unique<Helper>(task, n, &remaining_,
                                                      false, std::move(lock));
    auto* st = &helper->subtask;
    r_.write_to(st, n, amax, w, opts);
    st->on_finished(std::move(helper));
  }

  void close(event::Task* task, const base::Options& opts) override {
    r_.close(task, opts);
  }

 private:
  struct Helper : public event::Callback {
    event::Task* const task;
    std::size_t* const n;
    std::size_t* const remaining;
    const bool eof;
    base::Lock lock;
    event::Task subtask;

    Helper(event::Task* t, std::size_t* n, std::size_t* r, bool e,
           base::Lock lk) noexcept : task(t),
                                     n(n),
                                     remaining(r),
                                     eof(e),
                                     lock(std::move(lk)) {
      task->add_subtask(&subtask);
    }

    base::Result run() override {
      CHECK_GE(*remaining, *n);
      *remaining -= *n;
      lock.unlock();
      if (!event::propagate_failure(task, &subtask)) {
        if (eof)
          task->finish(base::Result::eof());
        else
          task->finish_ok();
      }
      return base::Result();
    }
  };

  const Reader r_;
  mutable std::mutex mu_;
  std::size_t remaining_;
};

class StringOrBufferReader : public ReaderImpl {
 public:
  StringOrBufferReader(ConstBuffer buf) noexcept : buf_(buf),
                                                   pos_(0),
                                                   closed_(false) {}
  StringOrBufferReader(std::string str) noexcept : str_(std::move(str)),
                                                   buf_(ConstBuffer(str_)),
                                                   pos_(0),
                                                   closed_(false) {}

  ~StringOrBufferReader() noexcept override { mu_.lock(); }

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  bool is_buffered() const noexcept override { return true; }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override {
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
                const Writer& w, const base::Options& opts) override {
    if (!prologue(task, n, max, w)) return;

    auto lock = base::acquire_lock(mu_);
    if (closed_) {
      task->finish(reader_closed());
      return;
    }

    const char* ptr = buf_.data() + pos_;
    std::size_t len = buf_.size() - pos_;
    if (len > max) len = max;

    auto helper =
        base::backport::make_unique<Helper>(task, n, &pos_, std::move(lock));
    auto* st = &helper->subtask;
    w.write(st, n, ptr, len, opts);
    st->on_finished(std::move(helper));
  }

  void close(event::Task* task, const base::Options& opts) override {
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
  struct Helper : public event::Callback {
    event::Task* const task;
    std::size_t* const n;
    std::size_t* const pos;
    base::Lock lock;
    event::Task subtask;

    Helper(event::Task* t, std::size_t* n, std::size_t* p,
           base::Lock lk) noexcept : task(t),
                                     n(n),
                                     pos(p),
                                     lock(std::move(lk)) {
      task->add_subtask(&subtask);
    }

    base::Result run() override {
      *pos += *n;
      lock.unlock();
      event::propagate_result(task, &subtask);
      return base::Result();
    }
  };

  const std::string str_;
  const ConstBuffer buf_;
  mutable std::mutex mu_;
  std::size_t pos_;
  bool closed_;
};

class NullReader : public ReaderImpl {
 public:
  NullReader() noexcept = default;

  std::size_t ideal_block_size() const noexcept override { return 64; }

  bool is_buffered() const noexcept override { return true; }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override {
    if (!prologue(task, out, n, min, max)) return;
    base::Result r;
    if (min > 0) r = base::Result::eof();
    *n = 0;
    task->finish(std::move(r));
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w, const base::Options& opts) override {
    if (!prologue(task, n, max, w)) return;
    *n = 0;
    task->finish_ok();
  }

  void close(event::Task* task, const base::Options& opts) override {
    if (prologue(task)) task->finish_ok();
  }
};

class ZeroReader : public ReaderImpl {
 public:
  ZeroReader() noexcept = default;

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  bool is_buffered() const noexcept override { return true; }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override {
    if (!prologue(task, out, n, min, max)) return;
    if (max > 0) ::bzero(out, max);
    *n = max;
    task->finish_ok();
  }

  void close(event::Task* task, const base::Options& opts) override {
    if (prologue(task)) task->finish_ok();
  }
};

class FDReader : public ReaderImpl {
 public:
  struct Op {
    virtual ~Op() noexcept = default;
    virtual void cancel() = 0;
    virtual bool process(FDReader* reader) = 0;
  };

  struct ReadOp : public Op {
    event::Task* const task;
    char* const out;
    std::size_t* const n;
    const std::size_t min;
    const std::size_t max;
    const base::Options options;
    event::Handle rdevt;

    ReadOp(event::Task* t, char* o, std::size_t* n, std::size_t mn,
           std::size_t mx, base::Options opts) noexcept
        : task(t),
          out(o),
          n(n),
          min(mn),
          max(mx),
          options(std::move(opts)) {}
    void cancel() override { task->cancel(); }
    bool process(FDReader* reader) override;
  };

  struct WriteToOp : public Op {
    event::Task* const task;
    std::size_t* const n;
    const std::size_t max;
    const Writer writer;
    const base::Options options;
    event::Handle rdevt;
    event::Handle wrevt;

    WriteToOp(event::Task* t, std::size_t* n, std::size_t mx, Writer w,
              base::Options opts) noexcept : task(t),
                                             n(n),
                                             max(mx),
                                             writer(std::move(w)),
                                             options(std::move(opts)) {}
    void cancel() override { task->cancel(); }
    bool process(FDReader* reader) override;
  };

  explicit FDReader(base::FD fd) noexcept : fd_(std::move(fd)), depth_(0) {}
  ~FDReader() noexcept override;

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override;
  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w, const base::Options& opts) override;
  void close(event::Task* task, const base::Options& opts) override;

  base::FD internal_readerfd() const override { return fd_; }

 private:
  void process(base::Lock& lock);
  base::Result wake(event::Set set);
  base::Result arm(event::Handle* evt, const base::FD& fd, event::Set set,
                   const base::Options& o);

  const base::FD fd_;
  mutable std::mutex mu_;
  std::condition_variable cv_;         // protected by mu_
  std::deque<std::unique_ptr<Op>> q_;  // protected by mu_
  std::vector<event::Handle> purge_;   // protected by mu_
  std::size_t depth_;                  // protected by mu_
};

FDReader::~FDReader() noexcept {
  VLOG(6) << "io::FDReader::~FDReader";
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

void FDReader::read(event::Task* task, char* out, std::size_t* n,
                    std::size_t min, std::size_t max,
                    const base::Options& opts) {
  if (!prologue(task, out, n, min, max)) return;
  auto lock = base::acquire_lock(mu_);
  VLOG(6) << "io::FDReader::read: min=" << min << ", max=" << max;
  q_.emplace_back(new ReadOp(task, out, n, min, max, opts));
  process(lock);
}

void FDReader::write_to(event::Task* task, std::size_t* n, std::size_t max,
                        const Writer& w, const base::Options& opts) {
  if (!prologue(task, n, max, w)) return;
  auto lock = base::acquire_lock(mu_);
  VLOG(6) << "io::FDReader::write_to: max=" << max;
  q_.emplace_back(new WriteToOp(task, n, max, w, opts));
  process(lock);
}

void FDReader::close(event::Task* task, const base::Options& opts) {
  VLOG(6) << "io::FDReader::close";
  base::Result r = fd_->close();
  if (prologue(task)) task->finish(std::move(r));
}

void FDReader::process(base::Lock& lock) {
  VLOG(4) << "io::FDReader::process: begin: q.size()=" << q_.size();

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
    VLOG(5) << "io::FDReader::process: consumed";
  }

  if (event::internal::is_shallow()) {
    auto p = std::move(purge_);
    for (auto& evt : p) evt.wait();
  }

  VLOG(4) << "io::FDReader::process: end";
}

base::Result FDReader::wake(event::Set set) {
  VLOG(6) << "woke io::FDReader, set=" << set;
  auto lock = base::acquire_lock(mu_);
  ++depth_;
  auto cleanup = base::cleanup([this] {
    --depth_;
    if (depth_ == 0) cv_.notify_all();
  });
  process(lock);
  return base::Result();
}

base::Result FDReader::arm(event::Handle* evt, const base::FD& fd,
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

bool FDReader::ReadOp::process(FDReader* reader) {
  VLOG(4) << "io::FDReader::ReadOp: begin: "
          << "*n=" << *n << ", "
          << "min=" << min << ", "
          << "max=" << max;

  // Disable any event and make sure someone waits on it.
  auto cleanup = base::cleanup([this, reader] {
    if (rdevt) {
      rdevt.disable().expect_ok(__FILE__, __LINE__);
      auto lock = base::acquire_lock(reader->mu_);
      reader->purge_.push_back(std::move(rdevt));
    }
  });

  // Check for cancellation
  if (!task->is_running()) {
    VLOG(4) << "io::FDReader::ReadOp: cancel";
    task->finish_cancel();
    return true;
  }

  const auto& rfd = reader->fd_;

  base::Result r;
  // Until we finish the read operation...
  while (*n < max) {
    // Attempt to read some data
    auto pair = rfd->acquire_fd();
    VLOG(5) << "io::FDReader::ReadOp: read: "
            << "fd=" << pair.first << ", "
            << "len=" << (max - *n);
    ssize_t len = ::read(pair.first, out + *n, max - *n);
    int err_no = errno;
    VLOG(6) << "io::FDReader::ReadOp: result=" << len;
    pair.second.unlock();

    // Check the return code
    if (len < 0) {
      // Interrupted by signal? Retry immediately
      if (err_no == EINTR) {
        VLOG(6) << "io::FDReader::ReadOp: EINTR";
        continue;
      }

      // No data for non-blocking read?
      // |min == 0| is success, otherwise reschedule for later
      if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
        VLOG(6) << "io::FDReader::ReadOp: EAGAIN";

        // If we've hit the minimum threshold, call it a day.
        if (*n >= min) break;

        // Register a callback for poll, if we didn't already.
        r = reader->arm(&rdevt, rfd, event::Set::readable_bit(), options);
        if (!r) break;

        cleanup.cancel();
        return false;
      }

      // Other error? Bomb out
      r = base::Result::from_errno(err_no, "read(2)");
      break;
    }
    if (len == 0) {
      VLOG(6) << "io::FDReader::ReadOp: EOF";
      if (*n < min) r = base::Result::eof();
      break;
    }
    *n += len;
  }
  VLOG(4) << "io::FDReader::ReadOp: end: "
          << "*n=" << *n << ", "
          << "r=" << r;
  task->finish(std::move(r));
  return true;
}

bool FDReader::WriteToOp::process(FDReader* reader) {
  VLOG(4) << "io::FDReader::WriteToOp: begin: "
          << "*n=" << *n << ", "
          << "max=" << max;

  // Disable any events and make sure someone waits on them.
  auto cleanup = base::cleanup([this, reader] {
    if (rdevt || wrevt) {
      rdevt.disable().expect_ok(__FILE__, __LINE__);
      wrevt.disable().expect_ok(__FILE__, __LINE__);
      auto lock = base::acquire_lock(reader->mu_);
      reader->purge_.push_back(std::move(rdevt));
      reader->purge_.push_back(std::move(wrevt));
    }
  });

  // Check for cancellation
  if (!task->is_running()) {
    VLOG(4) << "io::FDReader::WriteToOp: cancel";
    task->finish_cancel();
    return true;
  }

  auto xm = options.get<io::Options>().transfer_mode;
  if (xm == TransferMode::system_default) xm = default_transfer_mode();

  base::FD rfd = reader->fd_;
  base::FD wfd = writer.implementation()->internal_writerfd();
  base::Result r;

#if HAVE_SPLICE
  // Try using splice(2)
  if (xm >= TransferMode::splice && wfd) {
    while (*n < max) {
      std::size_t cmax = max - *n;
      if (cmax > kSpliceMax) cmax = kSpliceMax;

      auto pair0 = wfd->acquire_fd();
      auto pair1 = rfd->acquire_fd();
      VLOG(5) << "io::FDReader::WriteToOp: splice: "
              << "wfd=" << pair0.first << ", "
              << "rfd=" << pair1.first << ", "
              << "max=" << cmax << ", "
              << "*n=" << *n;
      ssize_t sent = ::splice(pair1.first, nullptr, pair0.first, nullptr, cmax,
                              SPLICE_F_NONBLOCK);
      int err_no = errno;
      VLOG(6) << "io::FDReader::WriteToOp: result=" << sent;
      pair1.second.unlock();
      pair0.second.unlock();

      // Check the return code
      if (sent < 0) {
        // Interrupted by signal? Retry immediately
        if (err_no == EINTR) {
          VLOG(6) << "io::FDReader::WriteToOp: EINTR";
          continue;
        }

        // splice(2) not implemented, at all?
        if (err_no == ENOSYS) {
          VLOG(6) << "io::FDReader::WriteToOp: ENOSYS";
          goto no_splice;
        }
        // splice(2) not implemented, for this pair of fds?
        if (err_no == EINVAL) {
          VLOG(6) << "io::FDReader::WriteToOp: EINVAL";
          goto no_splice;
        }

        // No data for read, or full buffers for write?
        // Reschedule for later
        if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
          VLOG(6) << "io::FDReader::WriteToOp: EAGAIN";

          // Errno doesn't distinguish "reader is empty" from "writer is
          // full", so schedule on both of them.
          r = reader->arm(&rdevt, rfd, event::Set::readable_bit(), options);
          if (!r) break;
          r = reader->arm(&wrevt, wfd, event::Set::writable_bit(), options);
          if (!r) break;

          cleanup.cancel();
          return false;
        }

        // Other error? Bomb out
        r = base::Result::from_errno(err_no, "sendfile(2)");
        break;
      }
      if (sent == 0) {
        VLOG(6) << "io::FDReader::WriteToOp: EOF";
        break;
      }
      *n += sent;
    }
    goto finish;
  }
no_splice:
#endif  // HAVE_SPLICE

#ifdef HAVE_SENDFILE
  // Try using sendfile(2)
  if (xm >= TransferMode::sendfile && wfd) {
    while (*n < max) {
      std::size_t cmax = max - *n;
      if (cmax > kSendfileMax) cmax = kSendfileMax;

      auto pair0 = wfd->acquire_fd();
      auto pair1 = rfd->acquire_fd();
      VLOG(5) << "io::FDReader::WriteToOp: sendfile: "
              << "wfd=" << pair0.first << ", "
              << "rfd=" << pair1.first << ", "
              << "max=" << cmax << ", "
              << "*n=" << *n;
      ssize_t sent = ::sendfile(pair0.first, pair1.first, nullptr, cmax);
      int err_no = errno;
      VLOG(6) << "io::FDReader::WriteToOp: result=" << sent;
      pair1.second.unlock();
      pair0.second.unlock();

      // Check the return code
      if (sent < 0) {
        // Interrupted by signal? Retry immediately
        if (err_no == EINTR) {
          VLOG(6) << "io::FDReader::WriteToOp: EINTR";
          continue;
        }

        // sendfile(2) not implemented, at all?
        if (err_no == ENOSYS) {
          VLOG(6) << "io::FDReader::WriteToOp: ENOSYS";
          goto no_sendfile;
        }
        // sendfile(2) not implemented, for this pair of fds?
        if (err_no == EINVAL) {
          VLOG(6) << "io::FDReader::WriteToOp: EINVAL";
          goto no_sendfile;
        }

        // No data for read, or full buffers for write?
        // Reschedule for later
        if (err_no == EAGAIN || err_no == EWOULDBLOCK) {
          VLOG(6) << "io::FDReader::WriteToOp: EAGAIN";

          // Errno doesn't distinguish "reader is empty" from "writer is
          // full", so schedule on both of them.
          r = reader->arm(&rdevt, rfd, event::Set::readable_bit(), options);
          if (!r) break;
          r = reader->arm(&wrevt, wfd, event::Set::writable_bit(), options);
          if (!r) break;

          cleanup.cancel();
          return false;
        }

        // Other error? Bomb out
        r = base::Result::from_errno(err_no, "sendfile(2)");
        break;
      }
      if (sent == 0) {
        VLOG(6) << "io::FDReader::WriteToOp: EOF";
        break;
      }
      *n += sent;
    }
    goto finish;
  }
no_sendfile:
#endif  // HAVE_SENDFILE

  // Nothing else left to try
  r = base::Result::not_implemented();

finish:
  VLOG(4) << "io::FDReader::WriteToOp: end: "
          << "*n=" << *n << ", "
          << "r=" << r;
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
    const base::Options options;
    event::Task subtask;
    std::size_t subn;

    Op(event::Task* t, char* o, std::size_t* n, std::size_t mn, std::size_t mx,
       base::Options opts) noexcept : task(t),
                                      out(o),
                                      n(n),
                                      min(mn),
                                      max(mx),
                                      options(std::move(opts)),
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
        if (event::propagate_failure(task, &subtasks[i])) {
          propagated = true;
          break;
        }
      }
      if (!propagated) task->finish_ok();
      delete this;
      return base::Result();
    }
  };

  explicit MultiReader(std::vector<Reader> vec) noexcept : vec_(std::move(vec)),
                                                           pass_(0),
                                                           curr_(0) {}

  std::size_t ideal_block_size() const noexcept override {
    return kDefaultIdealBlockSize;  // TODO: calculate LCM(vec[0], vec[1], ...)
  }

  bool is_buffered() const noexcept override {
    for (const auto& r : vec_) {
      if (!r.is_buffered()) return false;
    }
    return true;
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override {
    if (!prologue(task, out, n, min, max)) return;
    auto lock = base::acquire_lock(mu_);
    q_.emplace_back(new Op(task, out, n, min, max, opts));
    VLOG(6) << "io::MultiReader::read";
    process(lock);
  }

  void close(event::Task* task, const base::Options& opts) override {
    std::size_t size = vec_.size();
    auto* helper = new CloseHelper(task, size);
    auto closure = [helper] { return helper->run(); };
    event::Task* subtasks = helper->subtasks.get();
    for (std::size_t i = 0; i < size; ++i) {
      event::Task* st = subtasks + i;
      task->add_subtask(st);
      vec_[i].close(st, opts);
      st->on_finished(event::callback(closure));
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

      bool completed = false;
      while (pass_ > 0) {
        lock.unlock();
        auto cleanup1 = base::cleanup([&lock] { lock.lock(); });
        completed = op->process(this);
        cleanup1.run();
        if (completed) break;
        --pass_;
      }
      pass_ = 1;
      if (!completed) {
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
    event::propagate_result(task, &subtask);
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
  r.read(&subtask, subout, &subn, submin, submax, options);

  auto closure = [reader] {
    auto lock = base::acquire_lock(reader->mu_);
    reader->process(lock);
    return base::Result();
  };
  subtask.on_finished(event::callback(closure));
  return false;
}

class BufferedReader : public ReaderImpl {
 public:
  BufferedReader(Reader r, PoolPtr p, std::size_t max_buffers) noexcept
      : r_(std::move(r)),
        chain_(std::move(p), max_buffers),
        closed_(false) {
    chain_.set_rdfn([this](const base::Options& opts) { fill_callback(opts); });
  }

  BufferedReader(Reader r, PoolPtr p) noexcept : r_(std::move(r)),
                                                 chain_(std::move(p)),
                                                 closed_(false) {
    chain_.set_rdfn([this](const base::Options& opts) { fill_callback(opts); });
  }

  BufferedReader(Reader r, std::size_t buffer_size, std::size_t max_buffers)
      : r_(std::move(r)), chain_(buffer_size, max_buffers), closed_(false) {
    chain_.set_rdfn([this](const base::Options& opts) { fill_callback(opts); });
  }

  BufferedReader(Reader r) : r_(std::move(r)), chain_(), closed_(false) {
    chain_.set_rdfn([this](const base::Options& opts) { fill_callback(opts); });
  }

  std::size_t ideal_block_size() const noexcept override {
    return chain_.pool()->buffer_size();
  }

  bool is_buffered() const noexcept override { return true; }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override {
    chain_.read(task, out, n, min, max, opts);
  }

  void close(event::Task* task, const base::Options& opts) override {
    CHECK_NOTNULL(task);
    auto lock = base::acquire_lock(mu_);
    auto r = reader_closed();
    if (closed_) {
      if (task->start()) task->finish(std::move(r));
      return;
    }
    chain_.fail_writes(r);
    chain_.fail_reads(r);
    chain_.flush();
    chain_.process();
    closed_ = true;
    lock.unlock();
    r_.close(task, opts);
  }

 private:
  struct FillHelper : public event::Callback {
    event::Task task;
    BufferedReader* self;
    OwnedBuffer buffer;
    std::size_t length;
    std::size_t n;

    explicit FillHelper(BufferedReader* s, const base::Options& opts) noexcept
        : self(s),
          buffer(self->chain_.pool()->take()),
          length(self->chain_.optimal_fill()),
          n(0) {
      self->r_.read(&task, buffer.data(), &n, 1, length, opts);
    }

    base::Result run() override {
      base::Result r;
      if (task.result_will_throw()) {
        r = base::Result::unknown();
      } else {
        r = task.result();
      }
      if (r) {
        std::size_t nn = 0;
        self->chain_.fill(&nn, buffer.data(), n);
        CHECK_EQ(nn, n);
      } else {
        self->chain_.fail_reads(r);
      }
      self->chain_.pool()->give(std::move(buffer));
      self->chain_.process();
      return base::Result();
    }
  };

  void fill_callback(const base::Options& opts) {
    auto helper = base::backport::make_unique<FillHelper>(this, opts);
    auto* h = helper.get();
    h->task.on_finished(std::move(helper));
  }

  const Reader r_;
  Chain chain_;
  mutable std::mutex mu_;
  bool closed_;
};
}  // inline namespace implementation

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

Reader fdreader(base::FD fd) {
  return Reader(std::make_shared<FDReader>(std::move(fd)));
}

Reader multireader(std::vector<Reader> readers) {
  return Reader(std::make_shared<MultiReader>(std::move(readers)));
}

template <typename... Args>
static Reader make_bufferedreader(Args&&... args) {
  return Reader(std::make_shared<BufferedReader>(std::forward<Args>(args)...));
}

Reader bufferedreader(Reader r, PoolPtr pool, std::size_t max_buffers) {
  return make_bufferedreader(std::move(r), std::move(pool), max_buffers);
}

Reader bufferedreader(Reader r, PoolPtr pool) {
  return make_bufferedreader(std::move(r), std::move(pool));
}

Reader bufferedreader(Reader r, std::size_t buffer_size,
                      std::size_t max_buffers) {
  return make_bufferedreader(std::move(r), buffer_size, max_buffers);
}

Reader bufferedreader(Reader r) { return make_bufferedreader(std::move(r)); }

base::Result reader_closed() {
  return base::Result::from_errno(EBADF, "io::Reader is closed");
}

}  // namespace io
