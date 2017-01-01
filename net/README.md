# Networking

## net/options.h

RC.  Provides `net::Options`, which holds options for how to perform network
operations.  In addition to holding an instance of `io::Options`, this class
includes knobs for the `SO_REUSEADDR` and `IPV6_V6ONLY` socket options, and
for IPv4-vs-IPv6 address selection preferences.

## net/addr.h

RC.  Provides `net::Addr`, a class that provides an abstraction for network
addresses.  Also defines `net::AddrImpl`, a base class for implementations of
`net::Addr`.

## net/ip.h

RC.  Provides `net::IP`, a value type for IPv4 and IPv6 addresses, and
`net::CIDR`, a value type for address masks adhering to the CIDR standard.

## net/sockopt.h

RC.  Provides `net::SockOpt`, a value type representing a specific socket
option.

Example: `net::sockopt_reuseaddr` represents `(SOL_SOCKET, SO_REUSEADDR)`.

## net/conn.h

RC.  Provides `net::Conn` and `net::ListenConn`, which provides a higher-level
API over sockets and other bi-directional data pipes.  Also defines
`net::ConnImpl` and `net::ListenConnImpl`, which are the base classes for
socket implementations.  One pre-made implementation of each (native BSD
sockets) is available in `net/connfd.h`.

## net/protocol.h

RC.  Defines `net::Protocol`, a base class for protocols.  A protocol is
identified by a string like "tcp4" (TCP over IPv4) or "unixgram" (`AF_UNIX`
socket in datagram mode); each protocol roughly corresponds to a triple of
numbers passed to `socket(2)`.

## net/registry.h

RC.  Provides `net::Registry`, which provides a higher-level API over a
library of available protocols.

Also provides `net::system_registry()`, a shared instance of `net::Registry`
where certain `net::Protocol`s get automatically installed.

## net/net.h

RC.  Provides high-level functions for performing network operations, such as
`net::resolve()` (turn names into addresses) and `net::dial()` (connect to an
address).

(These are thin wrappers around the methods of `net::system_registry()`.)

## net/inet.h

RC.  Provides `net::inetaddr()`, a function which turns a protocol name, a
`net::IP`, and a port number into a `net::Addr`.  Also provides
`net::inetprotocol()`, which returns the singleton instance of `net::Protocol`
for IPv4 and IPv6 sockets.

## net/unix.h

RC.  Provides `net::unixaddr()`, a function which turns a protocol name and a
path into a `net::Addr`.  Also provides `net::unixprotocol()`, which returns
the singleton instance of `net::Protocol` for `AF_UNIX` sockets.

## net/fake.h

BETA.  Provides a toolkit for registering your own fake protocol that uses
in-process "sockets" (no FDs are involved).  Useful in tests of higher-level
socket code.

## net/connfd.h

**NOTE**: This header is only useful to implementers of `net::Protocol`
subclasses that use native BSD sockets.

RC.  Provides `net::fdconnreader()`, `net::fdconnwriter()`, `net::fdconn()`,
and `net::fdlistenconn()`, each of which takes a file descriptor for a native
BSD socket and returns an implementation of its respective class.

Also provides `net::FDProtocol`, a partially-specified base class for
subclasses of `net::Protocol` that use native BSD sockets.

## net/testing.h

**NOTE**: This header is only useful to implementers of `net::Protocol`
subclasses.

BETA.  Provides `net::TestListenAndDial()`, a function that takes a
`net::Protocol` implementation and subjects it to a vigorous workout.

