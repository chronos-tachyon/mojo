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

## Strings

### base/concat.h

STABLE.  Provides `base::concat(...)` and `base::concat_to(str, ...)`
functions for building strings through concatenation of pieces.  Also defines
an interface for objects to declare themselves stringable.

## Results

### base/result.h
### base/result_testing.h

RC.  Provides a `base::Result` value type representing success or failure of
an operation, plus macros for checking such values in tests.

## Logging

### base/logging.h

BETA.  Provides macros for generating log messages, including logged
assertions.  Also provides an API for consuming/intercepting logs.

(The API for log generation is RC, but the log consumption API is BETA.)

### base/debug.h

STABLE.  Check and change the global debugging mode.  Assertions are always
checked, but the global debugging mode affects whether or not assertion
failures are fatal.

## Date/Time

### base/duration.h

RC.  Provides a `base::Duration` value type to represent a span of time.

### base/time.h

RC.  Provides `base::Time` and `base::MonotonicTime` value types to represent
an instant of time.

### base/clock.h

RC.  Provides `base::Clock` and `base::MonotonicClock` classes to obtain time
values.  Also defines `base::ClockImpl` and `base::MonotonicClockImpl`, which
are the base classes for clock implementations.

Also provides `base::wallclock_now()` and `base::monotonic_now()` convenience
functions for accessing the system clock.

### base/clockfake.h

RC.  Provides `base::FakeClock` and `base::FakeMonotonicClock` for testing.

### base/stopwatch.h

RC.  Provides a `base::Stopwatch` class for measuring spans of elapsed time.

## Files

### base/fd.h

RC.  Provides `base::FDHolder` (a wrapper for file descriptors) and `base::FD`
(a smart pointer for `base::FDHolder`).  Wrapping a file descriptor ensures
that it gets closed when no longer in use, and also protects against the
recycling of file descriptor numbers.

## Event Loops and Asynchronous Programming

**NOTE**: Most users will be interested in the `event::Manager` API.

**NOTE**: These APIs are frequently used with the I/O APIs.

### event/callback.h
### event/task.h
### event/dispatcher.h

RC.  Defines the following:

* `event::Callback`, a base class for oneshot callback functions;
* `event::Task`, a class that allows observers to watch an operation; and
* `event::Dispatcher`, a base class for running callbacks.

Three basic models of `event::Dispatcher` are provided: **inline** dispatchers
run their callbacks immediately; **async** dispatchers defer their callbacks
until the next turn of the event loop; and **threaded** dispatchers run their
callbacks ASAP on a thread pool.

Quick example:

    // Create a new asynchronous dispatcher.
    event::DispatcherPtr dispatcher;
    event::DispatcherOptions do;
    do.set_type(event::DispatcherType::async_dispatcher);
    CHECK_OK(event::new_dispatcher(&dispatcher, do));

    // Start an asynchronous operation.
    event::Task task;
    operation(&task, dispatcher);

    // Schedules an action to run when the operation completes.
    bool done = false;
    task->on_finish(event::callback([&task, &done] {
      CHECK_OK(task.result());
      done = true;
      return base::Result();
    });

    // Spin the event loop.
    while (!done) dispatcher->donate(false);

### event/set.h
### event/poller.h

RC.  Provides `event::Poller`, a base class that abstracts over event polling
techniques, and helper class `event::Set`, a value type representing a set of
events.

### event/data.h
### event/handler.h
### event/manager.h

RC.  Defines the following:

* `event::Data`, an aggregate struct containing data about an event;
* `event::Handler`, a base class for multishot callback functions;
* `event::Manager`, a class that integrates polling and dispatching;
* `event::FileDescriptor`, `event::Signal`, `event::Timer`, and
  `event::Generic`, each of which binds an event to an `event::Handler`.

Example:

    // Create a new asynchronous event manager.
    event::Manager m;
    event::ManagerOptions mo;
    mo.set_async_mode();
    CHECK_OK(event::new_manager(&m, mo));

    // Set up a non-blocking socket.
    int fdnum = accept4(..., SOCK_NONBLOCK);
    base::FD fd = base::FDHolder::make(fdnum);

    // Operate on socket asynchronously.
    event::FileDescriptor evt;
    bool done = false;
    auto closure = [&](event::Data data) {
      if (data.events.error()) {
        // Got an error from the remote end!
        CHECK_OK(evt.disable());  // No more handler callbacks, please.
        CHECK_OK(fd->close());
        return;
      }
      if (data.events.readable()) {
        // Socket has data to read!
        ...;  // Call read(2) in a loop until -1, errno=EAGAIN.
      }
      if (data.events.writable()) {
        // Socket can accept writes!
        ...;  // Call write(2) in a loop until -1, errno=EAGAIN.
      }
    }
    CHECK_OK(m.fd(&evt, fd, event::Set::all_bits(), event::handler(closure)));

    // Spin the event loop.
    while (!done) m.donate(false);

## Generic I/O

### io/options.h

RC.  Provides `io::Options`, which holds options for how to perform I/O.
Important knobs include setting the preferred I/O blocksize, specifying the
`event::Manager` on which async I/O will be scheduled, and (advanced
performance feature) specifying a buffer pool to use.

### io/reader.h
### io/writer.h

RC.  Provides `io::Reader` and `io::Writer`, which provide a higher-level API
for reading and writing data.  Also defines `io::ReaderImpl` and
`io::WriterImpl`, which are the base classes for reader and writer
implementations, respectively.  A number of pre-made implementations are
available, including ones that direct I/O to strings and to file descriptors.

### io/util.h

RC.  Provides `io::copy()`, a function that knows the most efficient way to
copy data from an `io::Reader` to an `io::Writer`.

### io/pipe.h

RC.  Provides `io::make_pipe()`, a function that produces an `io::Reader` /
`io::Writer` linked pair, such that data written to the Writer will become
available to the Reader.

### io/testing.h

BETA.  Provides `io::MockReader`, an `io::ReaderImpl` that allows strict API
mocking.

### io/buffer.h

RC.  Provides `io::ConstBuffer` and `io::Buffer` (pointers to existing
fixed-size byte buffers), `io::OwnedBuffer` (a newly allocated fixed-size byte
buffer), and `io::BufferPool` (a free pool of `io::OwnedBuffer` objects).

## Networking

### net/options.h

RC.  Provides `net::Options`, which holds options for how to perform network
operations.  In addition to holding an instance of `io::Options`, this class
includes knobs for the `SO_REUSEADDR` and `IPV6_V6ONLY` socket options, and
for IPv4-vs-IPv6 address selection preferences.

### net/addr.h

RC.  Provides `net::Addr`, a class that provides an abstraction for network
addresses.  Also defines `net::AddrImpl`, a base class for implementations of
`net::Addr`.

### net/ip.h

RC.  Provides `net::IP`, a value type for IPv4 and IPv6 addresses, and
`net::CIDR`, a value type for address masks adhering to the CIDR standard.

### net/sockopt.h

RC.  Provides `net::SockOpt`, a value type representing a specific socket
option.

Example: `net::sockopt_reuseaddr` represents `(SOL_SOCKET, SO_REUSEADDR)`.

### net/conn.h

RC.  Provides `net::Conn` and `net::ListenConn`, which provides a higher-level
API over sockets and other bi-directional data pipes.  Also defines
`net::ConnImpl` and `net::ListenConnImpl`, which are the base classes for
socket implementations.  One pre-made implementation of each (native BSD
sockets) is available in `net/connfd.h`.

### net/protocol.h

RC.  Defines `net::Protocol`, a base class for protocols.  A protocol is
identified by a string like "tcp4" (TCP over IPv4) or "unixgram" (`AF_UNIX`
socket in datagram mode); each protocol roughly corresponds to a triple of
numbers passed to `socket(2)`.

### net/registry.h

RC.  Provides `net::Registry`, which provides a higher-level API over a
library of available protocols.

Also provides `net::system_registry()`, a shared instance of `net::Registry`
where certain `net::Protocol`s get automatically installed.

### net/net.h

RC.  Provides high-level functions for performing network operations, such as
`net::resolve()` (turn names into addresses) and `net::dial()` (connect to an
address).

(These are thin wrappers around the methods of `net::system_registry()`.)

### net/inet.h

RC.  Provides `net::inetaddr()`, a function which turns a protocol name, a
`net::IP`, and a port number into a `net::Addr`.  Also provides
`net::inetprotocol()`, which returns the singleton instance of `net::Protocol`
for IPv4 and IPv6 sockets.

### net/unix.h

RC.  Provides `net::unixaddr()`, a function which turns a protocol name and a
path into a `net::Addr`.  Also provides `net::unixprotocol()`, which returns
the singleton instance of `net::Protocol` for `AF_UNIX` sockets.

### net/fake.h

BETA.  Provides a toolkit for registering your own fake protocol that uses
in-process "sockets" (no FDs are involved).  Useful in tests of higher-level
socket code.

### net/connfd.h

**NOTE**: This header is only useful to implementers of `net::Protocol`
subclasses that use native BSD sockets.

RC.  Provides `net::fdconnreader()`, `net::fdconnwriter()`, `net::fdconn()`,
and `net::fdlistenconn()`, each of which takes a file descriptor for a native
BSD socket and returns an implementation of its respective class.

Also provides `net::FDProtocol`, a partially-specified base class for
subclasses of `net::Protocol` that use native BSD sockets.

### net/testing.h

**NOTE**: This header is only useful to implementers of `net::Protocol`
subclasses.

BETA.  Provides `net::TestListenAndDial()`, a function that takes a
`net::Protocol` implementation and subjects it to a vigorous workout.

## Miscellaneous

### base/backport.h

STABLE.  Provides backports of C++14 and/or C++17 features.

Currently provides `base::backport::integer_sequence`,
`base::backport::make_integer_sequence`, `base::backport::index_sequence`, and
`base::backport::make_index_sequence` (for indexing tuples).

### base/cleanup.h

STABLE.  RAII class to run code upon leaving a scope.

Quick example:

    static int foo = 0;
    ++foo;
    auto cleanup = base::cleanup([&] { --foo; });

### base/token.h

STABLE.  `base::token_t` value type representing a unique opaque token.

Quick example:

    base::token_t token1 = base::next_token();
    base::token_t token2 = base::next_token();
    CHECK_EQ(token1, token1);           // tokens are comparable
    CHECK_NE(token1, token2);           // tokens are distinct from each other
    CHECK_NE(token1, base::token_t());  // default is distinct from any other
    CHECK_NE(token2, base::token_t());
    auto hash = std::hash<base::token_t>()(token1);  // tokens are hashable

### base/util.h

BETA.  Miscellaneous small utility functions.  These will probably retain
their current APIs but move to new headers.
