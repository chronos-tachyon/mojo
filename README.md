# mojo

Mojo is a toolkit for C++ [Bazel](https://bazel.build/) users.  The long-term
goal is for Mojo to be for C++ what the Python or Go standard libraries are for
their respective languages.  The immediate focus for early versions of Mojo is
to build the infrastructure for network servers powered by asynchronous
event-driven dispatch.  This will eventually include a basic but standards
compliant HTTP/1.1 and HTTP/2.0 web server library.

While Mojo is original code, it takes inspiration from a number of sources:

* The C++ codebase at a previous employer
* The [Go standard library](https://golang.org/pkg/)
* The [Python standard library](https://docs.python.org/3/library/index.html)

Exported features fall into one of the following categories:

* FROZEN: APIs are frozen.  No changes except bug fixes.
* STABLE: Existing APIs are stable, but new APIs may be added.
* RC: APIs are provisional and subject to change.
* BETA: APIs are in flux.

## Basic Toolkit

See [`base/README.md`](base/README.md).  This package includes helpers for
logging, for building message strings, for returning standardized error codes,
for measuring and manipulating time, and for many other small tasks.

## Event Loops and Asynchronous Programming

See [`event/README.md`](event/README.md).  This package is loosely inspired by
the Python 3 `async` module, but also provides thread pools for parallel
processing.  Most significantly, the package provides `event::Manager`, a
class for asynchronously handling events from file descriptors, signals,
timers, and other sources.

## Generic I/O

See [`io/README.md`](io/README.md).  This package takes inspiration from the
Go `io` package, providing a standardized API for arbitrary byte streams,
`io::Reader` and `io::Writer`.

## Networking

See [`net/README.md`](net/README.md).  This package takes inspiration from the
Go `net` package, providing networking abstractions like `net::Addr` and
`net::Conn`.

