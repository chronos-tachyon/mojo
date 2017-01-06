// net/protocol.h - Abstraction for individual network protocols
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_PROTOCOL_H
#define NET_PROTOCOL_H

#include <memory>
#include <string>
#include <vector>

#include "base/result.h"
#include "base/token.h"
#include "event/task.h"
#include "net/addr.h"
#include "net/conn.h"
#include "net/options.h"

struct sockaddr;  // forward declaration

namespace net {

// Protocol is the abstract base class for network protocols.
//
// Protocol instances do the following things:
// - They interpret "struct sockaddr" raw address data as net::Addr objects
// - They parse human-readable address data as net::Addr objects
// - They resolve named addresses to lists of net::Addr objects
// - They create listen sockets
// - They connect to peer sockets
//
class Protocol {
 protected:
  Protocol() noexcept = default;

 public:
  // Protocol is neither copyable nor moveable.
  Protocol(const Protocol&) = delete;
  Protocol(Protocol&&) = delete;
  Protocol& operator=(const Protocol&) = delete;
  Protocol& operator=(Protocol&&) = delete;

  virtual ~Protocol() noexcept = default;

  // Returns true iff this Protocol knows how to interpret "struct sockaddr"
  // values with an sa_family of |family|.
  //
  // Example: AF_INET6
  virtual bool interprets(int family) const = 0;

  // Interprets |sa| as a |len|-byte sockaddr and populates |out|.
  //
  // PRECONDITION: |interprets(sa.sa_family)| returned true
  // POSTCONDITION: |out->protocol_type() == p| or an error was returned
  virtual base::Result interpret(Addr* out, ProtocolType p, const sockaddr* sa,
                                 int len) const = 0;

  // Returns true iff this Protocol knows how to deal with |protocol|:
  // listening, dialing, address parsing, and address resolving.
  //
  // Example: "tcp6"
  virtual bool supports(const std::string& protocol) const = 0;

  // Parses |address| as a human-readable |protocol| resolved address string.
  //
  // PRECONDITION: |supports(protocol)| returned true
  // POSTCONDITION: |out->protocol() == protocol| or an error was returned
  virtual base::Result parse(Addr* out, const std::string& protocol,
                             const std::string& address) const = 0;

  // Resolves |address| as a human-readable |protocol| address.
  //
  // The list of resolved addresses may include a mix of protocols, e.g. "tcp4"
  // and "tcp6".  However, this Protocol MUST be able to handle all of them.
  //
  // The list of resolved addresses MUST have a consistent ProtocolType.
  //
  // The list of resolved addresses SHOULD be placed in the order in which they
  // will be tried.
  //
  // PRECONDITION: |supports(protocol)| returned true
  // POSTCONDITION: |supports(out->at(i).protocol())| for all indices |i|
  // POSTCONDITION: |out->at(i).protocol_type() == p|
  //                for some ProtocolType |p| and for all indices |i|
  virtual void resolve(event::Task* task, std::vector<Addr>* out,
                       const std::string& protocol, const std::string& address,
                       const base::Options& opts) = 0;

  // Starts listening on |bind|.
  //
  // PRECONDITION: |supports(bind.protocol())| returned true
  virtual void listen(event::Task* task, ListenConn* out, const Addr& bind,
                      const base::Options& opts, AcceptFn fn) = 0;

  // Connects from |bind| to |peer|.
  //
  // PRECONDITION: |supports(peer.protocol())| returned true
  // PRECONDITION: |bind.protocol() == peer.protocol() || !bind|
  virtual void dial(event::Task* task, Conn* out, const Addr& peer,
                    const Addr& bind, const base::Options& opts) = 0;
};

}  // namespace net

#endif  // NET_PROTOCOL_H
