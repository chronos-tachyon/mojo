// io/writer.h - API for writing data to a sink
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_WRITER_H
#define IO_WRITER_H

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

#include "base/endian.h"
#include "base/result.h"
#include "event/task.h"
#include "io/buffer.h"
#include "io/common.h"
#include "io/options.h"

namespace io {

class Reader;  // forward declaration

// WriterImpl is the base class for implementations of the Writer API.
class WriterImpl {
 protected:
  WriterImpl() noexcept = default;

 public:
  // Sanity-check helper for implementations of |write|.
  //
  // Typical usage:
  //
  //    void write(event::Task* task, std::size_t* n,
  //               const char* ptr, std::size_t len,
  //               const base::Options& opts) override {
  //      if (!prologue(task, n, ptr, len)) return;
  //      ...;  // actual implementation
  //      task->finish(result);
  //    }
  //
  static bool prologue(event::Task* task, std::size_t* n, const char* ptr,
                       std::size_t len);

  // Sanity-check helper for implementations of |read_from|.
  //
  // Typical usage:
  //
  //    void read_from(event::Task* task, std::size_t* n,
  //                   std::size_t max, const io::Reader& r,
  //                   const base::Options& opts) override {
  //      if (!prologue(task, n, max, r)) return;
  //      ...;  // actual implementation
  //      task->finish(result);
  //    }
  //
  static bool prologue(event::Task* task, std::size_t* n, std::size_t max,
                       const Reader& r);

  // Sanity-check helper for implementations of |close|.
  //
  // Typical usage:
  //
  //    void close(event::Task* task,
  //               const base::Options& opts) override {
  //      if (!prologue(task)) return;
  //      ...;  // actual implementation
  //      task->finish(result);
  //    }
  //
  static bool prologue(event::Task* task);

  // WriterImpls are neither copyable nor moveable.
  WriterImpl(const WriterImpl&) = delete;
  WriterImpl(WriterImpl&&) = delete;
  WriterImpl& operator=(const WriterImpl&) = delete;
  WriterImpl& operator=(WriterImpl&&) = delete;

  // Closes the Writer, if not already closed, and frees resources.
  virtual ~WriterImpl() noexcept = default;

  // Returns the block size which results in efficient writes.  For best
  // performance, write buffer sizes should be in multiples of this size.
  virtual std::size_t ideal_block_size() const noexcept = 0;

  // Returns true if this Writer has buffering.
  virtual bool is_buffered() const noexcept { return false; }

  // Writes |len| bytes out of the buffer at |ptr|.
  // - ALWAYS sets |*n| to the number of bytes successfully written
  //   ~ In the case of an error, |*n| is the number of bytes *known* to have
  //     been written, and may not be exact!  However, implementations should
  //     strive to advance the current write offset by |*n| exactly
  // - |*n == len|, unless there was an error
  //   ~ When implementing this interface in terms of the write(2) API,
  //     this means you MUST retry your write(2) calls in a loop until
  //     (a) the sum equals |len|, or (b) an error is encountered
  // - May be synchronous: implementations may block until the call is complete
  // - May be asynchronous: implementations may use an event::Manager to
  //   write data to a slow destination, e.g. the network
  // - Implementations should strive to be asynchronous
  //
  // THREAD SAFETY: Implementations of this function MUST be thread-safe.
  //
  virtual void write(event::Task* task, std::size_t* n, const char* ptr,
                     std::size_t len, const base::Options& opts) = 0;

  // OPTIONAL. Copies up to |max| bytes from |r| into this Writer.
  // - NEVER copies more than |max| bytes
  // - ALWAYS sets |*n| to the number of bytes successfully written
  //   ~ In the case of an error, |*n| is the number of bytes *known* to have
  //     been written, and may not be exact!  However, implementations should
  //     strive to advance the current read and write offsets by |*n| exactly
  // - May be synchronous: implementations may block until the call is complete
  // - May be asynchronous: implementations may use an event::Manager to
  //   write data to a slow destination, e.g. the network
  // - Implementations should strive to be asynchronous
  //
  // THREAD SAFETY: Implementations of this function MUST be thread-safe.
  //
  virtual void read_from(event::Task* task, std::size_t* n, std::size_t max,
                         const Reader& r, const base::Options& opts);

  // Flushes this Writer's buffers, if any.
  // - May be a no-op: if this Writer doesn't use any buffers,
  //   then the default no-op behavior is a valid implementation.
  // - May be synchronous: implementations may block until the call is complete
  // - May be asynchronous: implementations may use an event::Manager to
  //   perform work asynchronously, e.g. flushing data to a remote host
  // - Implementations should strive to be asynchronous
  //
  virtual void flush(event::Task* task, const base::Options& opts);

  // Syncs all previous writes to durable storage, if applicable.
  // - Implies flush.
  // - May be a no-op: if this writer doesn't write to durable storage,
  //   then the default flush-only behavior is a valid implementation.
  // - May be synchronous: implementations may block until the call is complete
  // - May be asynchronous: implementations may use an event::Manager to
  //   perform work asynchronously, e.g. waiting for a remote host to sync
  virtual void sync(event::Task* task, const base::Options& opts);

  // Closes this Writer, potentially freeing resources.
  // - Implies flush and sync.
  // - May be synchronous: implementations may block until the call is complete
  // - May be asynchronous: implementations may use an event::Manager to
  //   perform work asynchronously, e.g. flushing data to a remote host
  // - Implementations should strive to be asynchronous
  //
  // THREAD SAFETY: Implementations of this function MUST be thread-safe.
  //
  virtual void close(event::Task* task, const base::Options& opts) = 0;

  // FOR INTERNAL USE ONLY.  DO NOT CALL DIRECTLY.
  //
  // Notes for implementers follow.
  //
  // Returns an FD suitable as the target of sendfile(2) or splice(2), or
  // returns null if there is no such suitable FD.
  //
  // WARNING: A "suitable" FD means one where a direct write of data is
  // acceptable, bypassing the |write()| and |read_from()| methods entirely.
  //
  // Examples of non-suitable FDs:
  // - Implementations using pwrite(2) and a userspace file offset
  // - Implementations of TLS, SSL, or other cryptographic stream protocols
  // - Implementations that add any sort of protocol framing
  //
  // If you do not have a suitable FD on hand, just return |nullptr|.
  //
  virtual base::FD internal_writerfd() const { return nullptr; }
};

// Writer is a handle to a writable I/O stream.
//
// A Writer typically points at an I/O stream, and therefore exists in the
// "non-empty" state.  In contrast, a Writer without a stream exists in the
// "empty" state.  A default-constructed Writer is empty, as is a Writer on
// which the |reset()| method is called.
//
// I/O streams are reference counted.  When the last Writer referencing a
// stream is destroyed or becomes empty, then the stream is closed.
//
// Most method calls are illegal to call on an empty Writer.
//
class Writer {
 public:
  using Pointer = std::shared_ptr<WriterImpl>;

  // Writer is constructible from an implementation.
  Writer(Pointer ptr) noexcept : ptr_(std::move(ptr)) {}

  // Writer is default constructible, starting in the empty state.
  Writer() noexcept : ptr_() {}

  // Writer is copyable and moveable.
  // - These copy or move the handle, not the stream itself.
  Writer(const Writer&) = default;
  Writer(Writer&&) noexcept = default;
  Writer& operator=(const Writer&) = default;
  Writer& operator=(Writer&&) noexcept = default;

  // Resets this Writer to the empty state.
  void reset() noexcept { ptr_.reset(); }

  // Swaps this Writer with another.
  void swap(Writer& other) noexcept { ptr_.swap(other.ptr_); }

  // Returns true iff this Writer is non-empty.
  explicit operator bool() const noexcept { return !!ptr_; }

  // Asserts that this Writer is non-empty.
  void assert_valid() const;

  // Returns this Writer's I/O stream implementation.
  const Pointer& implementation() const { return ptr_; }
  Pointer& implementation() { return ptr_; }

  // Returns the preferred block size for this Writer's I/O.
  std::size_t ideal_block_size() const {
    assert_valid();
    return ptr_->ideal_block_size();
  }

  // Returns true if this Writer has buffering.
  //
  // Writers without buffering should be wrapped in a buffered writer before
  // attempting any byte- or line-oriented I/O.
  bool is_buffered() const {
    assert_valid();
    return ptr_->is_buffered();
  }

  // Standard write {{{

  // Writes up to |len| bytes from the buffer at |ptr|.
  // - See |WriterImpl::write| for details of the API contract.
  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len,
             const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->write(task, n, ptr, len, opts);
  }

  // Like |write| above, but writes from a std::string.
  void write(event::Task* task, std::size_t* n, const std::string& str,
             const base::Options& opts = base::default_options()) const {
    write(task, n, str.data(), str.size(), opts);
  }

  // Synchronous versions of the functions above.
  base::Result write(std::size_t* n, const char* ptr, std::size_t len,
                     const base::Options& opts = base::default_options()) const;
  base::Result write(std::size_t* n, const std::string& str,
                     const base::Options& opts = base::default_options()) const;

  // }}}
  // Write a single integer {{{

  // Writes a single 1-, 2-, 4-, or 8-byte unsigned integer.
  void write_u8(event::Task* task, uint8_t in,
                const base::Options& opts = base::default_options()) const;
  void write_u16(event::Task* task, uint16_t in, const base::Endian* endian,
                 const base::Options& opts = base::default_options()) const;
  void write_u32(event::Task* task, uint32_t in, const base::Endian* endian,
                 const base::Options& opts = base::default_options()) const;
  void write_u64(event::Task* task, uint64_t in, const base::Endian* endian,
                 const base::Options& opts = base::default_options()) const;

  // Writes a single 1-, 2-, 4-, or 8-byte signed 2's-complement integer.
  void write_s8(event::Task* task, int8_t in,
                const base::Options& opts = base::default_options()) const;
  void write_s16(event::Task* task, int16_t in, const base::Endian* endian,
                 const base::Options& opts = base::default_options()) const;
  void write_s32(event::Task* task, int32_t in, const base::Endian* endian,
                 const base::Options& opts = base::default_options()) const;
  void write_s64(event::Task* task, int64_t in, const base::Endian* endian,
                 const base::Options& opts = base::default_options()) const;

  // Writes a variable-length integer encoded in Protocol Buffer format.
  void write_uvarint(event::Task* task, uint64_t in,
                     const base::Options& opts = base::default_options()) const;
  void write_svarint(event::Task* task, int64_t in,
                     const base::Options& opts = base::default_options()) const;
  void write_svarint_zigzag(
      event::Task* task, int64_t in,
      const base::Options& opts = base::default_options()) const;

  // Synchronous versions of the functions above.
  base::Result write_u8(
      uint8_t in, const base::Options& opts = base::default_options()) const;
  base::Result write_u16(
      uint16_t in, const base::Endian* endian,
      const base::Options& opts = base::default_options()) const;
  base::Result write_u32(
      uint32_t in, const base::Endian* endian,
      const base::Options& opts = base::default_options()) const;
  base::Result write_u64(
      uint64_t in, const base::Endian* endian,
      const base::Options& opts = base::default_options()) const;
  base::Result write_s8(
      int8_t in, const base::Options& opts = base::default_options()) const;
  base::Result write_s16(
      int16_t in, const base::Endian* endian,
      const base::Options& opts = base::default_options()) const;
  base::Result write_s32(
      int32_t in, const base::Endian* endian,
      const base::Options& opts = base::default_options()) const;
  base::Result write_s64(
      int64_t in, const base::Endian* endian,
      const base::Options& opts = base::default_options()) const;
  base::Result write_uvarint(
      uint64_t in, const base::Options& opts = base::default_options()) const;
  base::Result write_svarint(
      int64_t in, const base::Options& opts = base::default_options()) const;
  base::Result write_svarint_zigzag(
      int64_t in, const base::Options& opts = base::default_options()) const;

  // }}}
  // Copy directly from Reader to Writer {{{

  // Attempts to efficiently copy up to |max| bytes of |r| into this Writer.
  // NOTE: This function is OPTIONAL, i.e. it may return NOT_IMPLEMENTED.
  //       See io::copy in io/util.h for a user-friendly interface.
  void read_from(event::Task* task, std::size_t* n, std::size_t max,
                 const Reader& r,
                 const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->read_from(task, n, max, r, opts);
  }

  // Synchronous version of |read_from| above.
  base::Result read_from(
      std::size_t* n, std::size_t max, const Reader& r,
      const base::Options& opts = base::default_options()) const;

  // }}}
  // Flush, Sync, Close {{{

  // Flushes this Writer's buffers, if any.
  void flush(event::Task* task,
             const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->flush(task, opts);
  }

  // Syncs all previous writes to permanent storage, if applicable.
  void sync(event::Task* task,
            const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->sync(task, opts);
  }

  // Closes this Writer, potentially freeing resources.
  void close(event::Task* task,
             const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->close(task, opts);
  }

  // Synchronous versions of the functions above.
  base::Result flush(const base::Options& opts = base::default_options()) const;
  base::Result sync(const base::Options& opts = base::default_options()) const;
  base::Result close(const base::Options& opts = base::default_options()) const;

  // }}}

 private:
  Pointer ptr_;
};

inline void swap(Writer& a, Writer& b) noexcept { a.swap(b); }
inline bool operator==(const Writer& a, const Writer& b) noexcept {
  return a.implementation() == b.implementation();
}
inline bool operator!=(const Writer& a, const Writer& b) noexcept {
  return !(a == b);
}

using WriteFn = std::function<void(event::Task*, std::size_t*, const char*,
                                   std::size_t, const base::Options&)>;
using SyncWriteFn = std::function<base::Result(
    std::size_t*, const char*, std::size_t, const base::Options&)>;

// Returns a Writer that wraps the given functor(s).
Writer writer(WriteFn wfn, CloseFn cfn);
Writer writer(SyncWriteFn wfn, SyncCloseFn cfn);
inline Writer writer(WriteFn wfn) {
  return writer(std::move(wfn), NoOpClose());
}
inline Writer writer(SyncWriteFn wfn) {
  return writer(std::move(wfn), NoOpClose());
}

// Given a Writer |w|, returns a new Writer which turns |close()| into a no-op
// but forwards all other method calls to |w|.
Writer ignore_close(Writer w);

// Returns a Writer that appends bytes to a std::string.
Writer stringwriter(std::string* str);

// Returns a Writer that writes bytes into a Buffer, updating |*n|.
Writer bufferwriter(Buffer buf, std::size_t* n);
inline Writer bufferwriter(char* ptr, std::size_t len, std::size_t* n) {
  return bufferwriter(Buffer(ptr, len), n);
}

// Returns a Writer that throws away everything it receives.
// Optionally takes a pointer to a size_t, recording the # of bytes discarded.
Writer discardwriter(std::size_t* /*nullable*/ n = nullptr);

// Returns a Writer that simulates a full disk.
Writer fullwriter();

// Returns a Writer that writes bytes to a file descriptor.
Writer fdwriter(base::FD fd);

// Wraps a Writer in I/O buffering.
Writer bufferedwriter(Writer w, PoolPtr pool, std::size_t max_buffers);
Writer bufferedwriter(Writer w, PoolPtr pool);
Writer bufferedwriter(Writer w, std::size_t buffer_size,
                      std::size_t max_buffers);
Writer bufferedwriter(Writer w);

// Returns an archetypal error result for performing I/O on a closed io::Writer.
base::Result writer_closed();

// Returns an archetypal error result for performing I/O on an io::Writer that
// has run out of available storage space.
base::Result writer_full();

}  // namespace io

#endif  // IO_WRITER_H
