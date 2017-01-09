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

The full documentation is at [mojo.tools](https://mojo.tools/).
