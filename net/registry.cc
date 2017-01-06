// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/registry.h"

#include <sys/socket.h>

#include <algorithm>
#include <stdexcept>

#include "base/logging.h"

static base::Result family_not_supp() {
  return base::Result::not_implemented("address family not supported");
}

static base::Result proto_not_supp() {
  return base::Result::not_implemented("network protocol not supported");
}

namespace net {

void Registry::add(base::token_t* t, prio_t prio,
                   std::shared_ptr<Protocol> ptr) {
  base::token_t token = base::next_token();
  items_.emplace_back(prio, token, std::move(ptr));
  std::sort(items_.begin(), items_.end());
  if (t) *t = token;
}

void Registry::remove(base::token_t t) {
  auto it = items_.end(), begin = items_.begin();
  while (it != begin) {
    --it;
    if (it->token == t) {
      items_.erase(it);
      break;
    }
  }
}

bool Registry::interprets(int family) const {
  for (const auto& item : items_) {
    if (item.ptr->interprets(family)) return true;
  }
  return false;
}

base::Result Registry::interpret(Addr* out, ProtocolType p, const sockaddr* sa,
                                 int len) const {
  CHECK_NOTNULL(out);
  for (const auto& item : items_) {
    if (item.ptr->interprets(sa->sa_family)) {
      return item.ptr->interpret(out, p, sa, len);
    }
  }
  return family_not_supp();
}

bool Registry::supports(const std::string& protocol) const {
  for (const auto& item : items_) {
    if (item.ptr->supports(protocol)) return true;
  }
  return false;
}

base::Result Registry::parse(Addr* out, const std::string& protocol,
                             const std::string& address) const {
  CHECK_NOTNULL(out);
  for (const auto& item : items_) {
    if (item.ptr->supports(protocol)) {
      return item.ptr->parse(out, protocol, address);
    }
  }
  return proto_not_supp();
}

void Registry::resolve(event::Task* task, std::vector<Addr>* out,
                       const std::string& protocol, const std::string& address,
                       const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  for (const auto& item : items_) {
    if (item.ptr->supports(protocol)) {
      item.ptr->resolve(task, out, protocol, address, opts);
      return;
    }
  }
  if (task->start()) task->finish(proto_not_supp());
}

void Registry::listen(event::Task* task, ListenConn* out, const Addr& bind,
                      const base::Options& opts, AcceptFn fn) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  CHECK(bind);
  CHECK(fn);
  for (const auto& item : items_) {
    if (item.ptr->supports(bind.protocol())) {
      item.ptr->listen(task, out, bind, opts, std::move(fn));
      return;
    }
  }
  if (task->start()) task->finish(proto_not_supp());
}

void Registry::dial(event::Task* task, Conn* out, const Addr& peer,
                    const Addr& bind, const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  CHECK(peer);
  if (bind) CHECK_EQ(bind.protocol(), peer.protocol());
  for (const auto& item : items_) {
    if (item.ptr->supports(peer.protocol())) {
      item.ptr->dial(task, out, peer, bind, opts);
      return;
    }
  }
  if (task->start()) task->finish(proto_not_supp());
}

base::Result Registry::resolve(std::vector<Addr>* out,
                               const std::string& protocol,
                               const std::string& address,
                               const base::Options& opts) const {
  event::Task task;
  resolve(&task, out, protocol, address, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result Registry::listen(ListenConn* out, const Addr& bind,
                              const base::Options& opts, AcceptFn fn) const {
  event::Task task;
  listen(&task, out, bind, opts, std::move(fn));
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result Registry::dial(Conn* out, const Addr& peer, const Addr& bind,
                            const base::Options& opts) const {
  event::Task task;
  dial(&task, out, peer, bind, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

std::mutex& system_registry_mutex() {
  static std::mutex mu;
  return mu;
}

Registry& system_registry_mutable() {
  static Registry& ref = *new Registry;
  return ref;
}

const Registry& system_registry() { return system_registry_mutable(); }

}  // namespace net
