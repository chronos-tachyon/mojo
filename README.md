# mojo

Mojo is a toolkit for C++ [Bazel](https://bazel.build/) users.  The long-term
goal is for Mojo to be for C++ what the Python or Go standard libraries are for
their respective languages.  The immediate focus for early versions of Mojo is
to build the infrastructure for network servers powered by asynchronous
event-driven dispatch.  This will eventually include a basic but standards
compliant HTTP/1.1 and HTTP/2.0 web server library.

Exported features fall into one of the following categories:

* FROZEN: APIs are frozen.  No changes except bug fixes.
* STABLE: Existing APIs are stable, but new APIs may be added.
* RC: APIs are provisional and subject to change.
* BETA: APIs are in flux.

## Basic Toolkit

See [`base/README.md`](base/README.md).

## Event Loops and Asynchronous Programming

See [`event/README.md`](event/README.md).

## Generic I/O

See [`io/README.md`](io/README.md).

## Networking

See [`net/README.md`](net/README.md).

