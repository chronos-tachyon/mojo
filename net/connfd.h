// net/connfd.h - Partial implementation of native FD-based network connections
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_CONNFD_H
#define NET_CONNFD_H

#include <tuple>

#include "base/fd.h"
#include "io/reader.h"
#include "io/writer.h"
#include "net/addr.h"
#include "net/conn.h"
#include "net/protocol.h"

namespace net {

// FDProtocol is a partial implementation of Protocol for protocols that use
// native socket file descriptors.  It implements listen and dial, leaving only
// name resolution (interpret, parse, resolve) for the subclass to implement.
class FDProtocol : public Protocol {
 protected:
  // Returns a pointer to this Protocol... or, at least, to a Protocol that can
  // interpret "struct sockaddr" values from getsockname(2) and getpeername(2).
  virtual std::shared_ptr<Protocol> self() const = 0;

  // Returns the (domain, type, protocol) triple to pass to socket(2).
  // - The SOCK_CLOEXEC and SOCK_NONBLOCK flags will be automatically
  //   added as needed and should not be returned here.
  virtual std::tuple<int, int, int> socket_triple(
      const std::string& protocol) const = 0;

 public:
  void listen(event::Task* task, ListenConn* out, const Addr& bind,
              const base::Options& opts, AcceptFn fn) override;

  void dial(event::Task* task, Conn* out, const Addr& peer, const Addr& bind,
            const base::Options& opts) override;
};

// Returns an io::Reader that maps |close()| to |shutdown(SHUT_RD)|.
io::Reader fdconnreader(base::FD fd);

// Returns an io::Writer that maps |close()| to |shutdown(SHUT_WR)|.
io::Writer fdconnwriter(base::FD fd);

// Returns a net::Conn with the specified properties.
base::Result fdconn(Conn* out, Addr la, Addr ra, base::FD fd);

// Returns a net::ListenConn with the specified properties.
// - |p| must be capable of |interpret()|-ing the results of the getsockname(2)
//   and getpeername(2) functions
base::Result fdlistenconn(ListenConn* out, std::shared_ptr<Protocol> pr,
                          Addr aa, base::FD fd, const base::Options& opts,
                          AcceptFn fn);

}  // namespace net

#endif  // NET_CONNFD_H
