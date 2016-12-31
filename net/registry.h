// net/registry.h - Registers the installed network protocols
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_REGISTRY_H
#define NET_REGISTRY_H

#include <memory>
#include <string>
#include <vector>

#include "base/result.h"
#include "base/token.h"
#include "event/task.h"
#include "net/addr.h"
#include "net/conn.h"
#include "net/options.h"
#include "net/protocol.h"

namespace net {

// Registry is a clearinghouse for registering and finding network protocols.
class Registry {
 public:
  // Indicates a priority for a Protocol.
  // Larger numbers indicate a higher priority.
  // System protocols are installed at priority 50.
  using prio_t = unsigned int;

  // Registry is default constructible.
  Registry() = default;

  // Registry is copyable and moveable.
  Registry(const Registry&) = default;
  Registry(Registry&&) noexcept = default;
  Registry& operator=(const Registry&) = default;
  Registry& operator=(Registry&&) noexcept = default;

  // Swaps this Registry with another.
  void swap(Registry& x) noexcept { items_.swap(x.items_); }

  // Returns true iff this Registry has a non-empty list of Protocols.
  explicit operator bool() const noexcept { return !items_.empty(); }

  // Registers a Protocol at priority |prio|.
  // - If |t| is provided, it is set to a token identifying this registration
  void add(base::token_t* /*nullable*/ t, prio_t prio,
           std::shared_ptr<Protocol> ptr);

  // Undoes the previous registration that yielded |t|.
  void remove(base::token_t t);

  // Returns true iff this Registry has a Protocol that knows how to
  // interpret the given sockaddr family |family|.
  bool interprets(int family) const;

  // Interprets |sa| as a |len|-byte sockaddr, putting the result in |out|.
  base::Result interpret(Addr* out, ProtocolType p, const sockaddr* sa,
                         int len) const;

  // Returns true iff this Registry has a Protocol that knows how to parse
  // and resolve addresses for |protocol|.
  bool supports(const std::string& protocol) const;

  // Parses |address| as a human-readable |protocol| resolved address string.
  base::Result parse(Addr* out, const std::string& protocol,
                     const std::string& address) const;

  // Resolves |address| as a human-readable |protocol| address.
  void resolve(event::Task* task, std::vector<Addr>* out,
               const std::string& protocol, const std::string& address,
               const Options& opts = default_options()) const;

  // Starts listening on |bind|.
  void listen(event::Task* task, ListenConn* out, const Addr& bind,
              const Options& opts, AcceptFn fn) const;
  void listen(event::Task* task, ListenConn* out, const Addr& bind,
              AcceptFn fn) const {
    return listen(task, out, bind, default_options(), std::move(fn));
  }

  // Connects from |bind| to |peer|.
  void dial(event::Task* task, Conn* out, const Addr& peer, const Addr& bind,
            const Options& opts = default_options()) const;

  // Synchronous versions of the functions above.
  base::Result resolve(std::vector<Addr>* out, const std::string& protocol,
                       const std::string& address,
                       const Options& opts = default_options()) const;
  base::Result listen(ListenConn* out, const Addr& bind, const Options& opts,
                      AcceptFn fn) const;
  base::Result listen(ListenConn* out, const Addr& bind, AcceptFn fn) const {
    return listen(out, bind, default_options(), std::move(fn));
  }
  base::Result dial(Conn* out, const Addr& peer, const Addr& bind,
                    const Options& opts = default_options()) const;

 private:
  struct Item {
    prio_t prio;
    base::token_t token;
    std::shared_ptr<Protocol> ptr;

    Item(prio_t prio, base::token_t t, std::shared_ptr<Protocol> ptr) noexcept
        : prio(prio),
          token(t),
          ptr(std::move(ptr)) {}
    Item() noexcept : prio(0), token(), ptr() {}
  };

  friend inline bool operator<(const Item& a, const Item& b) {
    return (a.prio > b.prio) || (a.prio == b.prio && a.token < b.token);
  }

  std::vector<Item> items_;
};

// Registry is swappable.
inline void swap(Registry& a, Registry& b) noexcept { a.swap(b); }

// Accesses the process-wide Registry.
std::mutex& system_registry_mutex();
Registry& system_registry_mutable();
const Registry& system_registry();

}  // namespace net

#endif  // NET_REGISTRY_H
