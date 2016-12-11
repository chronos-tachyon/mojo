#ifndef IO_PIPE_H
#define IO_PIPE_H

#include "io/reader.h"
#include "io/writer.h"

namespace io {

void make_pipe(Reader* r, Options ro, Writer* w, Options wo);

}  // namespace io

#endif  // IO_PIPE_H
