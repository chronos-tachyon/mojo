// io/pipe.h - In-process I/O pipes
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_PIPE_H
#define IO_PIPE_H

#include "io/reader.h"
#include "io/writer.h"

namespace io {

struct Pipe {
  Reader read;
  Writer write;

  Pipe() noexcept = default;
  Pipe(Reader r, Writer w) noexcept : read(std::move(r)), write(std::move(w)) {}
};

Pipe make_pipe(PoolPtr pool, std::size_t max_buffers);
Pipe make_pipe(PoolPtr pool);
Pipe make_pipe(std::size_t buffer_size, std::size_t max_buffers);
Pipe make_pipe();

void make_pipe(Reader* r, Writer* w);  // backward compatibility

}  // namespace io

#endif  // IO_PIPE_H
