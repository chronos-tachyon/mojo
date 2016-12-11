// io/options.h - Configurable I/O behaviors
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_OPTIONS_H
#define IO_OPTIONS_H

#include <cstdint>

#include "event/manager.h"
#include "io/buffer.h"

namespace io {

// TransferMode determines how data should be copied from a Reader to a Writer.
enum class TransferMode : uint8_t {
  // Do read(2) and write(2) in a loop.
  read_write = 0,

  // Try using sendfile(2), or fall back to read_write.
  //
  // Note that the implementation may decide that sendfile(2) is not viable on
  // a particular OS version, or that it is not viable between a particular
  // pair of I/O endpoints. Hence the fallback.
  sendfile = 1,

  // Try using splice(2), or fall back to sendfile.
  //
  // Note that the implementation may decide that splice(2) is not viable on a
  // particular OS version, or that it is not viable between a particular pair
  // of I/O endpoints. Hence the fallback.
  splice = 2,

  // Let the system choose.
  system_default = 255,
};

class Options {
 private:
  enum bits {
    bit_blocksize = (1U << 0),
  };

 public:
  Options() noexcept : blocksize_(0),
                       transfer_(TransferMode::system_default),
                       has_(0) {}
  Options(const Options&) noexcept = default;
  Options(Options&&) noexcept = default;
  Options& operator=(const Options&) noexcept = default;
  Options& operator=(Options&&) noexcept = default;

  // Returns the event::Manager on which to perform async I/O.
  event::Manager manager() const {
    return manager_.or_system_manager();  // Always returns a Manager
  }
  void set_manager(event::Manager m) { manager_ = std::move(m); }

  // Returns an optional BufferPool to use for obtaining scratch buffers.
  // A BufferPool can reduce the number of allocations made during copies.
  // - The BufferPool buffer size must be at least as large as the block_size.
  BufferPool pool() const noexcept { return pool_; }
  void set_pool(BufferPool pool) noexcept { pool_ = pool; }

  // Returns the preferred I/O block size.
  //
  // If |.first == true|, then |.second| is the user-specified block size.
  // If |.first == false|, then I/O will use implementation-defined defaults.
  //
  // This value should almost certainly be a power of two.
  //
  std::pair<bool, std::size_t> block_size() const noexcept {
    return std::make_pair((has_ & bit_blocksize) != 0, blocksize_);
  }
  void reset_block_size() noexcept {
    has_ &= ~bit_blocksize;
    blocksize_ = 0;
  }
  void set_block_size(std::size_t n) noexcept {
    blocksize_ = n;
    has_ |= bit_blocksize;
  }

  // Returns which transfer mode should be used.
  // See the description of the TransferMode enum for more information.
  TransferMode transfer_mode() const noexcept { return transfer_; }
  void reset_transfer_mode() noexcept {
    transfer_ = TransferMode::system_default;
  }
  void set_transfer_mode(TransferMode value) noexcept { transfer_ = value; }

 private:
  event::Manager manager_;
  BufferPool pool_;
  std::size_t blocksize_;
  TransferMode transfer_;
  uint8_t has_;
};

// Returns the default Options. Thread-safe.
const Options& default_options() noexcept;

// Changes the default Options. THREAD-HOSTILE.
Options& mutable_default_options() noexcept;

}  // namespace io

#endif  // IO_OPTIONS_H
