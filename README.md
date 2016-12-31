# mojo

Mojo is a toolkit for C++ Bazel users.  The goal is for Mojo to be for C++ what the Python or Go standard libraries are for their respective languages.  The immediate focus for early versions of Mojo is to build the infrastructure for network servers powered by asynchronous event-driven dispatch.  This will eventually include a basic but standards compliant HTTP/1.1 and HTTP/2.0 web server library.

## Exports

Exported features fall into one of the following categories:

* FROZEN: APIs are frozen.  No changes except bug fixes.
* STABLE: Existing APIs are stable, but new APIs may be added.
* RC: APIs are provisional and subject to change.
* BETA: APIs are in flux.

### Strings

* base/concat.h

STABLE.  Provides `base::concat(...)` and `base::concat_to(str, ...)` functions for building strings through concatenation of pieces.  Also defines an interface for objects to declare themselves stringable.

### Results

* base/result.h
* base/result_testing.h

RC.  Provides a `base::Result` value type representing success or failure of an operation, plus macros for checking such values in tests.

### Logging

* base/logging.h

BETA.  Provides macros for generating log messages, including logged assertions.  Also provides an API for consuming/intercepting logs.

(The API for log generation is RC, but the log consumption API is BETA.)

* base/debug.h

STABLE.  Check and change the global debugging mode.  Assertions are always checked, but the global debugging mode affects whether or not assertion failures are fatal.

### Date/Time

* base/duration.h
* base/time.h
* base/clock.h
* base/clockfake.h
* base/stopwatch.h

RC.  Provides a `base::Duration` value type to represent a span of time, `base::Time` and `base::MonotonicTime` value types to represent an instant of time, `base::Clock` and `base::MonotonicClock` classes to obtain time values, and a `base::Stopwatch` class for measuring spans of elapsed time.

### Files

* base/fd.h

RC.  Provides `base::FDHolder` (a wrapper for file descriptors) and `base::FD` (a smart pointer for `base::FDHolder`).  Wrapping a file descriptor ensures that it gets closed when no longer in use, and also protects against the recycling of file descriptor numbers.

### Miscellaneous

#### base/backport.h

STABLE.  Provides backports of C++14 and/or C++17 features.

Currently provides `base::backport::integer_sequence`, `base::backport::make_integer_sequence`, `base::backport::index_sequence`, and `base::backport::make_index_sequence` (for indexing tuples).

#### base/cleanup.h

STABLE.  RAII class to run code upon leaving a scope.

Example:

    static int foo = 0;
    ++foo;
    auto cleanup = base::cleanup([&] { --foo; });

#### base/token.h

STABLE.  `base::token_t` value type representing a unique opaque token.

#### base/util.h

BETA.  Miscellaneous small utility functions.  These will probably retain their current APIs but move to new headers.
