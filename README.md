# mojo

Mojo is a toolkit for C++ Bazel users.  The goal is for Mojo to be for C++ what the Python or Go standard libraries are for their respective languages.  The immediate focus for early versions of Mojo is to build the infrastructure for network servers powered by asynchronous event-driven dispatch.  This will eventually include a basic but standards compliant HTTP/1.1 and HTTP/2.0 web server library.

## Exports

Exported features fall into one of the following categories:

* FROZEN: APIs are frozen.  No changes except bug fixes.
* STABLE: Existing APIs are stable, but new APIs may be added.
* RC: APIs are provisional and subject to change.
* BETA: APIs are in flux.

### base/backport.h

STABLE.  Provides backports of C++14 and/or C++17 features.

Currently provides `base::backport::integer_sequence`, `base::backport::make_integer_sequence`, `base::backport::index_sequence`, and `base::backport::make_index_sequence` (for indexing tuples).

### base/cleanup.h

STABLE.  RAII class to run code upon leaving a scope.

Example:

    static int foo = 0;
    ++foo;
    auto cleanup = base::cleanup([&] { --foo; });

### base/clock.h and base/clockfake.h

RC.  Interface for obtaining `base::Time` and `base::MonotonicTime` values.

### base/concat.h

STABLE.  Concatenate strings and stringable objects.

### base/debug.h

STABLE.  Check and change the global debugging mode.

### base/duration.h

RC.  `base::Duration` value type representing a span of time.

### base/fd.h

RC.  Wrapper for file descriptors.  Ensures that the file descriptor gets closed when no longer in use, and protects against the recycling of file descriptor numbers.

### base/logging.h

BETA.  Facility for logging error messages.

API for generating logs is RC.  API for consuming/intercepting logs is BETA.

### base/result.h

RC.  `base::Result` value type representing success or failure of an operation.

### base/result_testing.h

STABLE.  Macros for checking `base::Result` values in tests.

### base/stopwatch.h

RC.  `base::Stopwatch` class for measuring spans of time.

### base/time.h

RC.  `base::Time` and `base::MonotonicTime` value types representing an instant of time.

### base/token.h

STABLE.  `base::token_t` value type representing a unique opaque token.

### base/util.h

BETA.  Miscellaneous small utility functions.  These will probably retain their current APIs but move to new headers.
