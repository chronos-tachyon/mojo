// io/pipe.h - In-process I/O pipes
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_PIPE_H
#define IO_PIPE_H

#include "io/reader.h"
#include "io/writer.h"

namespace io {

void make_pipe(Reader* r, Writer* w);

}  // namespace io

#endif  // IO_PIPE_H
