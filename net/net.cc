// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/net.h"

#include "net/registry.h"

namespace net {

base::Result interpret(Addr* out, ProtocolType p, const sockaddr* sa, int len) {
  return system_registry().interpret(out, p, sa, len);
}

base::Result parse(Addr* out, const std::string& protocol,
                   const std::string& address) {
  return system_registry().parse(out, protocol, address);
}

void resolve(event::Task* task, std::vector<Addr>* out,
             const std::string& protocol, const std::string& address,
             const Options& opts) {
  system_registry().resolve(task, out, protocol, address, opts);
}

void resolve(event::Task* task, std::vector<Addr>* out,
             const std::string& protocol, const std::string& address) {
  system_registry().resolve(task, out, protocol, address);
}

void listen(event::Task* task, ListenConn* out, const Addr& bind,
            const Options& opts, AcceptFn fn) {
  system_registry().listen(task, out, bind, opts, std::move(fn));
}

void listen(event::Task* task, ListenConn* out, const Addr& bind, AcceptFn fn) {
  system_registry().listen(task, out, bind, std::move(fn));
}

void dial(event::Task* task, Conn* out, const Addr& peer, const Addr& bind,
          const Options& opts) {
  system_registry().dial(task, out, peer, bind, opts);
}

void dial(event::Task* task, Conn* out, const Addr& peer, const Addr& bind) {
  system_registry().dial(task, out, peer, bind);
}

base::Result resolve(std::vector<Addr>* out, const std::string& protocol,
                     const std::string& address, const Options& opts) {
  return system_registry().resolve(out, protocol, address, opts);
}

base::Result resolve(std::vector<Addr>* out, const std::string& protocol,
                     const std::string& address) {
  return system_registry().resolve(out, protocol, address);
}

base::Result listen(ListenConn* out, const Addr& bind, const Options& opts,
                    AcceptFn fn) {
  return system_registry().listen(out, bind, opts, std::move(fn));
}

base::Result listen(ListenConn* out, const Addr& bind, AcceptFn fn) {
  return system_registry().listen(out, bind, std::move(fn));
}

base::Result dial(Conn* out, const Addr& peer, const Addr& bind,
                  const Options& opts) {
  return system_registry().dial(out, peer, bind, opts);
}

base::Result dial(Conn* out, const Addr& peer, const Addr& bind) {
  return system_registry().dial(out, peer, bind);
}

}  // namespace net
