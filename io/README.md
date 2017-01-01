# Generic I/O

## io/options.h

RC.  Provides `io::Options`, which holds options for how to perform I/O.
Important knobs include setting the preferred I/O blocksize, specifying the
`event::Manager` on which async I/O will be scheduled, and (advanced
performance feature) specifying a buffer pool to use.

## io/reader.h
## io/writer.h

RC.  Provides `io::Reader` and `io::Writer`, which provide a higher-level API
for reading and writing data.  Also defines `io::ReaderImpl` and
`io::WriterImpl`, which are the base classes for reader and writer
implementations, respectively.  A number of pre-made implementations are
available, including ones that direct I/O to strings and to file descriptors.

## io/util.h

RC.  Provides `io::copy()`, a function that knows the most efficient way to
copy data from an `io::Reader` to an `io::Writer`.

## io/pipe.h

RC.  Provides `io::make_pipe()`, a function that produces an `io::Reader` /
`io::Writer` linked pair, such that data written to the Writer will become
available to the Reader.

## io/testing.h

BETA.  Provides `io::MockReader`, an `io::ReaderImpl` that allows strict API
mocking.

## io/buffer.h

RC.  Provides `io::ConstBuffer` and `io::Buffer` (pointers to existing
fixed-size byte buffers), `io::OwnedBuffer` (a newly allocated fixed-size byte
buffer), and `io::BufferPool` (a free pool of `io::OwnedBuffer` objects).

