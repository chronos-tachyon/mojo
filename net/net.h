// net/net.h - Standalone networking functions
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_NET_H
#define NET_NET_H

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

// Interprets |sa| as a |len|-byte sockaddr, putting the result in |out|.
base::Result interpret(Addr* out, ProtocolType p, const sockaddr* sa, int len);

// Parses |address| as a human-readable |protocol| resolved address string.
base::Result parse(Addr* out, const std::string& protocol,
                   const std::string& address);

// Resolves |address| as a human-readable |protocol| address.
void resolve(event::Task* task, std::vector<Addr>* out,
             const std::string& protocol, const std::string& address,
             const base::Options& opts);
void resolve(event::Task* task, std::vector<Addr>* out,
             const std::string& protocol, const std::string& address);

// Starts listening on |bind|.
void listen(event::Task* task, ListenConn* out, const Addr& bind,
            const base::Options& opts, AcceptFn fn);
void listen(event::Task* task, ListenConn* out, const Addr& bind, AcceptFn fn);

// Connects from |bind| to |peer|.
void dial(event::Task* task, Conn* out, const Addr& peer, const Addr& bind,
          const base::Options& opts);
void dial(event::Task* task, Conn* out, const Addr& peer, const Addr& bind);

// Synchronous versions of the functions above.
base::Result resolve(std::vector<Addr>* out, const std::string& protocol,
                     const std::string& address, const base::Options& opts);
base::Result resolve(std::vector<Addr>* out, const std::string& protocol,
                     const std::string& address);
base::Result listen(ListenConn* out, const Addr& bind,
                    const base::Options& opts, AcceptFn fn);
base::Result listen(ListenConn* out, const Addr& bind, AcceptFn fn);
base::Result dial(Conn* out, const Addr& peer, const Addr& bind,
                  const base::Options& opts);
base::Result dial(Conn* out, const Addr& peer, const Addr& bind);

}  // namespace net

#endif  // NET_NET_H
