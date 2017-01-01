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

See [`event/README.md`](event/README.md).

## Generic I/O

See [`io/README.md`](io/README.md).

## Networking

See [`net/README.md`](net/README.md).

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

### base/mutex.h

RC.  Mutexes, locks, and other concurrency tools.

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
