// io/reader.h - API for reading data from a source
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_READER_H
#define IO_READER_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <type_traits>

#include "base/fd.h"
#include "base/result.h"
#include "event/task.h"
#include "io/buffer.h"
#include "io/common.h"
#include "io/options.h"

namespace io {

class Writer;  // forward declaration

// ReaderImpl is the base class for implementations of the Reader API.
class ReaderImpl {
 protected:
  ReaderImpl() noexcept = default;

 public:
  // Sanity-check helper for implementations of |read|.
  //
  // Typical usage:
  //
  //    void read(event::Task* task, char* out, std::size_t* n,
  //              std::size_t min, std::size_t max,
  //              const io::Options& opts) override {
  //      if (!prologue(task, out, n, min, max)) return;
  //      ...;  // actual implementation
  //      task->finish(result);
  //    }
  //
  static bool prologue(event::Task* task, char* out, std::size_t* n,
                       std::size_t min, std::size_t max);

  // Sanity-check helper for implementations of |write_to|.
  //
  // Typical usage:
  //
  //    void write_to(event::Task* task, std::size_t* n,
  //                  std::size_t max, const io::Writer& w,
  //                  const io::Options& opts) override {
  //      if (!prologue(task, n, max, w)) return;
  //      ...;  // actual implementation
  //      task->finish(result);
  //    }
  //
  static bool prologue(event::Task* task, std::size_t* n, std::size_t max,
                       const Writer& w);

  // Sanity-check helper for implementations of |close|.
  //
  // Typical usage:
  //
  //    void close(event::Task* task,
  //               const io::Options& opts) override {
  //      if (!prologue(task)) return;
  //      ...;  // actual implementation
  //      task->finish(result);
  //    }
  //
  static bool prologue(event::Task* task);

  // ReaderImpls are neither copyable nor moveable.
  ReaderImpl(const ReaderImpl&) = delete;
  ReaderImpl(ReaderImpl&&) = delete;
  ReaderImpl& operator=(const ReaderImpl&) = delete;
  ReaderImpl& operator=(ReaderImpl&&) = delete;

  // Closes the Reader, if not already closed, and frees resources.
  virtual ~ReaderImpl() noexcept = default;

  // Returns the block size which results in efficient reads.  For best
  // performance, read buffer sizes should be in multiples of this size.
  virtual std::size_t ideal_block_size() const noexcept = 0;

  // Reads up to |max| bytes into the buffer at |out|.
  // - NEVER reads more than |max| bytes
  // - ALWAYS sets |*n| to the number of bytes successfully read
  //   ~ It is advisable to set |*n = 0| at the top of the function, so that
  //     all error cases are covered, including exceptions
  //   ~ In the case of an error, |*n| is the number of bytes *known* to have
  //     been read, and may not be exact!  However, implementations should
  //     strive to advance the current read offset by |*n| exactly
  // - |*n >= min|, unless there was an error
  //   ~ If |*n < min| because the end of the stream was reached,
  //     it's an END_OF_FILE error
  // - May be synchronous: implementations may block until the call is complete
  // - May be asynchronous: implementations may use an event::Manager to
  //   read data from a slow source, e.g. the network
  // - Implementations should strive to be asynchronous
  //
  // Specifics for |min == 0 && max > 0|:
  // - MUST attempt to read some data
  // - MUST return with |*n == 0| if the end of the stream was reached
  // - MAY return with |*n == 0| if no data is available
  // - NEVER returns an END_OF_FILE error
  //
  // Specifics for |min == 0 && max == 0|:
  // - MAY check for filehandle/connection/etc. errors
  // - MAY return immediately
  //
  // THREAD SAFETY: Implementations of this function MUST be thread-safe.
  //
  virtual void read(event::Task* task, char* out, std::size_t* n,
                    std::size_t min, std::size_t max, const Options& opts) = 0;

  // OPTIONAL. Copies up to |max| bytes of this Reader's data into |w|.
  // - NEVER copies more than |max| bytes
  // - ALWAYS sets |*n| to the number of bytes successfully written
  //   ~ It is advisable to set |*n = 0| at the top of the function, so that
  //     all error cases are covered, including exceptions
  //   ~ In the case of an error, |*n| is the number of bytes *known* to have
  //     been written, and may not be exact!  However, implementations should
  //     strive to advance the current read and write offsets by |*n| exactly
  // - NEVER returns an END_OF_FILE result
  // - May be synchronous: implementations may block until the call is complete
  // - May be asynchronous: implementations may use an event::Manager to
  //   write data to a slow destination, e.g. the network
  // - Implementations should strive to be asynchronous
  //
  // THREAD SAFETY: Implementations of this function MUST be thread-safe.
  //
  virtual void write_to(event::Task* task, std::size_t* n, std::size_t max,
                        const Writer& w, const Options& opts);

  // Closes this Reader, potentially freeing resources.
  // - May be synchronous: implementations may block until the call is complete
  // - May be asynchronous: implementations may use an event::Manager to
  //   perform work asynchronously, e.g. flushing data to a remote host
  // - Implementations should strive to be asynchronous
  //
  // THREAD SAFETY: Implementations of this function MUST be thread-safe.
  //
  virtual void close(event::Task* task, const Options& opts) = 0;

  // FOR INTERNAL USE ONLY.  DO NOT CALL DIRECTLY.
  virtual base::FD internal_readerfd() const { return nullptr; }
};

// Reader is a handle to a readable I/O stream.
//
// A Reader typically points at an I/O stream, and therefore exists in the
// "non-empty" state.  In contrast, a Reader without a stream exists in the
// "empty" state.  A default-constructed Reader is empty, as is a Reader on
// which the |reset()| method is called.
//
// I/O streams are reference counted.  When the last Reader referencing a
// stream is destroyed or becomes empty, then the stream is closed.
//
// Most methods are illegal to call on an empty Reader.
//
class Reader {
 private:
  static constexpr std::size_t computed_min(std::size_t len) noexcept {
    return (len > 0) ? 1 : 0;
  }

 public:
  using Pointer = std::shared_ptr<ReaderImpl>;

  // Reader is constructible from an implementation.
  Reader(Pointer ptr) noexcept : ptr_(std::move(ptr)) {}

  // Reader is default constructible, starting in the empty state.
  Reader() noexcept : ptr_() {}

  // Reader is copyable and moveable.
  // - These copy or move the handle, not the stream itself.
  Reader(const Reader&) = default;
  Reader(Reader&&) noexcept = default;
  Reader& operator=(const Reader&) = default;
  Reader& operator=(Reader&&) noexcept = default;

  // Resets this Reader to the empty state.
  void reset() noexcept { ptr_.reset(); }

  // Swaps this Reader with another.
  void swap(Reader& other) noexcept { ptr_.swap(other.ptr_); }

  // Returns true iff this Reader is non-empty.
  explicit operator bool() const noexcept { return !!ptr_; }

  // Asserts that this Reader is non-empty.
  void assert_valid() const;

  // Returns this Reader's I/O stream implementation.
  const Pointer& implementation() const { return ptr_; }
  Pointer& implementation() { return ptr_; }

  // Returns the preferred block size for the I/O stream.
  std::size_t ideal_block_size() const {
    assert_valid();
    return ptr_->ideal_block_size();
  }

  // Fully qualified read {{{

  // Reads |min| to |max| bytes into the buffer at |out|, updating |n|.
  // - See |ReaderImpl::read| for details of the API contract.
  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const Options& opts = default_options()) const {
    assert_valid();
    ptr_->read(task, out, n, min, max, opts);
  }

  // Like |read| above, but reads into a std::string.
  void read(event::Task* task, std::string* out, std::size_t min,
            std::size_t max, const Options& opts = default_options()) const;

  // Synchronous versions of the functions above.
  base::Result read(char* out, std::size_t* n, std::size_t min, std::size_t max,
                    const Options& opts = default_options()) const;
  base::Result read(std::string* out, std::size_t min, std::size_t max,
                    const Options& opts = default_options()) const;

  // }}}
  // Read up to N bytes {{{

  // Reads up to |len| bytes into the buffer at |out|, updating |n|.
  void read(event::Task* task, char* out, std::size_t* n, std::size_t len,
            const Options& opts = default_options()) const {
    read(task, out, n, computed_min(len), len, opts);
  }

  // Like |read| above, but reads into a std::string.
  void read(event::Task* task, std::string* out, std::size_t len,
            const Options& opts = default_options()) const {
    read(task, out, computed_min(len), len, opts);
  }

  // Synchronous versions of the functions above.
  base::Result read(char* out, std::size_t* n, std::size_t len,
                    const Options& opts = default_options()) const {
    return read(out, n, computed_min(len), len, opts);
  }
  base::Result read(std::string* out, std::size_t len,
                    const Options& opts = default_options()) const {
    return read(out, computed_min(len), len, opts);
  }

  // }}}
  // Read exactly N bytes {{{

  // Reads exactly |len| bytes into the buffer at |out|, updating |*n|.
  void read_exactly(event::Task* task, char* out, std::size_t* n,
                    std::size_t len,
                    const Options& opts = default_options()) const {
    read(task, out, n, len, len, opts);
  }

  // Like |read_exactly| above, but reads into a std::string.
  void read_exactly(event::Task* task, std::string* out, std::size_t len,
                    const Options& opts = default_options()) const {
    read(task, out, len, len, opts);
  }

  // Synchronous versions of the functions above.
  base::Result read_exactly(char* out, std::size_t* n, std::size_t len,
                            const Options& opts = default_options()) const {
    return read(out, n, len, len, opts);
  }
  base::Result read_exactly(std::string* out, std::size_t len,
                            const Options& opts = default_options()) const {
    return read(out, len, len, opts);
  }

  // }}}
  // Copy directly from Reader to Writer {{{

  // Attempts to efficiently copy up to |max| bytes of this Reader into |w|.
  // NOTE: This function is OPTIONAL, i.e. it may return NOT_IMPLEMENTED.
  //       See io::copy in io/util.h for a user-friendly interface.
  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w,
                const Options& opts = default_options()) const {
    assert_valid();
    ptr_->write_to(task, n, max, w, opts);
  }

  // Synchronous version of |write_to| above.
  base::Result write_to(std::size_t* n, std::size_t max, const Writer& w,
                        const Options& opts = default_options()) const;

  // }}}
  // Close {{{

  // Closes this Reader, potentially freeing resources.
  void close(event::Task* task, const Options& opts = default_options()) const {
    assert_valid();
    ptr_->close(task, opts);
  }

  // Synchronous version of |close| above.
  base::Result close(const Options& opts = default_options()) const;

  // }}}

 private:
  Pointer ptr_;
};

inline void swap(Reader& a, Reader& b) noexcept { a.swap(b); }
inline bool operator==(const Reader& a, const Reader& b) noexcept {
  return a.implementation() == b.implementation();
}
inline bool operator!=(const Reader& a, const Reader& b) noexcept {
  return !(a == b);
}

using ReadFn =
    std::function<void(event::Task*, char*, std::size_t*, std::size_t,
                       std::size_t, const Options& opts)>;
using SyncReadFn = std::function<base::Result(
    char*, std::size_t*, std::size_t, std::size_t, const Options& opts)>;

// Returns a Reader that wraps the given functor(s).
Reader reader(ReadFn rfn, CloseFn cfn);
Reader reader(SyncReadFn rfn, SyncCloseFn cfn);
inline Reader reader(ReadFn rfn) { return reader(std::move(rfn), NoOpClose()); }
inline Reader reader(SyncReadFn rfn) {
  return reader(std::move(rfn), NoOpClose());
}

// Given a Reader |r|, returns a new Reader which turns |close()| into a no-op
// but forwards all other method calls to |r|.
Reader ignore_close(Reader r);

// Given a Reader |r|, returns a new Reader which reaches EOF after reading the
// first |max| bytes of |r|.
Reader limited_reader(Reader r, std::size_t max);

// Returns a Reader that produces bytes from a std::string.
Reader stringreader(std::string str);

// Returns a Reader that produces bytes from a ConstBuffer.
Reader bufferreader(ConstBuffer buf);
inline Reader bufferreader(const char* ptr, std::size_t len) {
  return bufferreader(ConstBuffer(ptr, len));
}

// Returns a Reader that's always at EOF.
Reader nullreader();

// Returns a Reader that yields an unending stream of '\0' NUL bytes.
Reader zeroreader();

// Returns a Reader that reads bytes from a file descriptor.
Reader fdreader(base::FD fd);

// Returns a Reader that concatenates multiple streams into one.
Reader multireader(std::vector<Reader> readers);

}  // namespace io

#endif  // IO_READER_H
