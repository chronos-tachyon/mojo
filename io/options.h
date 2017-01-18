// io/options.h - Configurable I/O behaviors
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_OPTIONS_H
#define IO_OPTIONS_H

#include <cstdint>

#include "base/options.h"
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

struct Options : public base::OptionsType {
  // Use this event::Manager to perform async I/O.
  event::Manager manager;

  // Use this io::Pool (if provided) for obtaining scratch buffers.
  PoolPtr pool;

  // Overrides the preferred I/O block size, or 0 to use the default.
  // - If non-zero, this value should almost certainly be a power of two.
  std::size_t block_size;

  // Determines how data should be copied from a Reader to a Writer.
  TransferMode transfer_mode;

  Options() noexcept : block_size(0),
                       transfer_mode(TransferMode::system_default) {}
  Options(const Options&) noexcept = default;
  Options(Options&&) noexcept = default;
  Options& operator=(const Options&) noexcept = default;
  Options& operator=(Options&&) noexcept = default;

  // Resets this io::Options to the default values.
  void reset() { *this = Options(); }
};

inline event::Manager get_manager(const base::Options& opts) {
  return opts.get<Options>().manager.or_system_manager();
}

}  // namespace io

#endif  // IO_OPTIONS_H
