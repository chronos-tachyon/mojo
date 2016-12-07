// io/writer.h - API for writing data to a sink
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_WRITER_H
#define IO_WRITER_H

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

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
  WriterImpl(Options o) noexcept : o_(std::move(o)) {}

 public:
  // Sanity-check helper for implementations of |write|.
  //
  // Typical usage:
  //
  //    void write(event::Task* task, std::size_t* n,
  //               const char* ptr, std::size_t len) override {
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
  //                   std::size_t max, Reader& r) override {
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
  //    void close(event::Task* task) override {
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
                     std::size_t len) = 0;

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
  virtual void read_from(event::Task* task, std::size_t* n,
                         std::size_t max, const Reader& r);

  // Closes this Writer, potentially freeing resources.
  // - May be synchronous: implementations may block until the call is complete
  // - May be asynchronous: implementations may use an event::Manager to
  //   perform work asynchronously, e.g. flushing data to a remote host
  // - Implementations should strive to be asynchronous
  //
  // THREAD SAFETY: Implementations of this function MUST be thread-safe.
  //
  virtual void close(event::Task* task) = 0;

  // Returns the minimum size which results in efficient writes.
  virtual std::size_t ideal_block_size() const noexcept { return 4096; }

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

  // Accesses the io::Options which were provided at construction time.
  const Options& options() const noexcept { return o_; }

 private:
  Options o_;
};

// Writer implements the user-facing portion of the Writer API.
class Writer {
 public:
  // Writers can be directly constructed from an implementation.
  Writer(std::shared_ptr<WriterImpl> ptr) noexcept : ptr_(std::move(ptr)) {}

  // Writers are default constructible, copyable, and moveable.
  Writer() noexcept : ptr_() {}
  Writer(const Writer&) = default;
  Writer(Writer&&) noexcept = default;
  Writer& operator=(const Writer&) = default;
  Writer& operator=(Writer&&) noexcept = default;

  // Invalidates this Writer.
  void reset() noexcept { ptr_.reset(); }

  // Swaps this Writer with another.
  void swap(Writer& other) noexcept { ptr_.swap(other.ptr_); }

  // Determines if this Writer has an implementation associated with it.
  explicit operator bool() const noexcept { return !!ptr_; }
  bool valid() const noexcept { return !!*this; }
  void assert_valid() const;

  // Obtains a pointer directly to the implementation.
  WriterImpl* implementation() const { return ptr_.get(); }

  // Returns the io::Options that were assigned to the Writer implementation at
  // the time it was created.
  const Options& options() const {
    if (ptr_) return ptr_->options();
    return default_options();
  }

  // Returns the event::Manager to use for this Writer's async I/O.
  event::Manager manager() const {
    if (ptr_) return ptr_->options().manager();
    return event::system_manager();
  }

  // Returns the preferred block size for I/O involving this Writer.
  std::size_t block_size() const {
    if (!ptr_) return 1;
    std::size_t blksz;
    bool has_blksz;
    std::tie(has_blksz, blksz) = ptr_->options().block_size();
    if (has_blksz) return blksz;
    return ptr_->ideal_block_size();
  }

  // Standard write {{{

  // Writes up to |len| bytes from the buffer at |ptr|.
  // - See |WriterImpl::write| for details of the API contract.
  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len) const {
    assert_valid();
    ptr_->write(task, n, ptr, len);
  }

  // Like |write| above, but writes from a std::string.
  void write(event::Task* task, std::size_t* n,
             const std::string& str) const {
    write(task, n, str.data(), str.size());
  }

  // Synchronous versions of the functions above.
  base::Result write(std::size_t* n, const char* ptr,
                     std::size_t len) const;
  base::Result write(std::size_t* n, const std::string& str) const;

  // }}}
  // Copy directly from Reader to Writer {{{

  // Attempts to efficiently copy up to |max| bytes of |r| into this Writer.
  // NOTE: This function is OPTIONAL, i.e. it may return NOT_IMPLEMENTED.
  //       See io::copy in io/util.h for a user-friendly interface.
  void read_from(event::Task* task, std::size_t* n, std::size_t max,
                 const Reader& r) const {
    assert_valid();
    ptr_->read_from(task, n, max, r);
  }

  // Synchronous version of |read_from| above.
  base::Result read_from(std::size_t* n, std::size_t max,
                         const Reader& r) const;

  // }}}
  // Close {{{

  // Closes this Writer, potentially freeing resources.
  void close(event::Task* task) const {
    assert_valid();
    ptr_->close(task);
  }

  // Synchronous version of |close| above.
  base::Result close() const;

  // }}}

 private:
  std::shared_ptr<WriterImpl> ptr_;
};

inline void swap(Writer& a, Writer& b) noexcept { a.swap(b); }
inline bool operator==(const Writer& a, const Writer& b) noexcept {
  return a.implementation() == b.implementation();
}
inline bool operator!=(const Writer& a, const Writer& b) noexcept {
  return !(a == b);
}

using WriteFn =
    std::function<void(event::Task*, std::size_t*, const char*, std::size_t)>;
using SyncWriteFn =
    std::function<base::Result(std::size_t*, const char*, std::size_t)>;

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
Writer discardwriter(std::size_t* n = nullptr, Options o = default_options());

// Returns a Writer that simulates a full disk.
Writer fullwriter(Options o = default_options());

// Returns a Writer that writes bytes to a file descriptor.
Writer fdwriter(base::FD fd, Options o = default_options());

}  // namespace io

#endif  // IO_WRITER_H
