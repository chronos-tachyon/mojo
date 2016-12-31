// net/addr.h - Abstraction for network addresses
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_ADDR_H
#define NET_ADDR_H

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>

#include "base/result.h"

namespace net {

// ProtocolType identifies which category a protocol falls into.
enum class ProtocolType : uint8_t {
  // Invalid value.
  unspecified = 0,

  // Raw "protocols" expose the raw packets, headers and all.
  //
  // Corresponds to: |SOCK_RAW|
  //
  raw = 1,

  // Datagram protocols transfer packets of data.
  // - They are NOT connection-oriented.
  // - They DO NOT offer reliable delivery.
  // - They DO NOT offer in-order delivery.
  // - They offer (minimal) integrity guarantees.
  //
  // Corresponds to: |SOCK_DGRAM|
  //
  datagram = 2,

  // Reliable Datagram protocols transfer packets of data.
  // - They are connection-oriented.
  // - They offer reliable delivery: lost packets are retransmitted.
  // - They DO NOT offer in-order delivery.
  // - They offer (minimal) integrity guarantees.
  //
  // Corresponds to: |SOCK_RDM|
  //
  // Neither AF_INET{,6} nor AF_UNIX support SOCK_RDM. This constant is not
  // likely to be useful except in the context of userspace network protocols.
  //
  rdm = 3,

  // Sequenced Packet protocols transfer packets of data.
  // - They are connection-oriented.
  // - They offer reliable delivery: lost packets are retransmitted.
  // - They offer in-order delivery: packets are buffered as needed.
  // - They offer (minimal) integrity guarantees.
  //
  // Corresponds to: |SOCK_SEQPACKET|
  //
  // AF_INET{,6} does not support SOCK_SEQPACKET. This constant is mostly
  // useful in the context of AF_UNIX sockets, or of userspace network
  // protocols.
  //
  seqpacket = 4,

  // Stream protocols transfer byte streams of data.
  // - They are connection-oriented.
  // - They offer reliable delivery: lost packets are retransmitted.
  // - They offer in-order delivery: packets are buffered as needed.
  // - They offer (minimal) integrity guarantees.
  //
  // Corresponds to: |SOCK_STREAM|
  //
  stream = 5,
};

// ProtocolType is stringable.
void append_to(std::string& out, ProtocolType p);
inline std::size_t length_hint(ProtocolType) { return 9; }

inline std::ostream& operator<<(std::ostream& o, ProtocolType p) {
  std::string tmp;
  append_to(tmp, p);
  return (o << tmp);
}

// AddrImpl is the abstract base class for Addr implementations.
class AddrImpl {
 protected:
  AddrImpl() noexcept = default;

 public:
  AddrImpl(const AddrImpl&) = delete;
  AddrImpl(AddrImpl&&) = delete;
  AddrImpl& operator=(const AddrImpl&) = delete;
  AddrImpl& operator=(AddrImpl&&) = delete;

  virtual ~AddrImpl() noexcept = default;
  virtual std::string protocol() const = 0;
  virtual ProtocolType protocol_type() const = 0;
  virtual std::string address() const = 0;
  virtual std::string ip() const { return std::string(); }
  virtual uint16_t port() const { return 0; }
  virtual std::pair<const void*, std::size_t> raw() const = 0;
};

// Addr represents a single network address.
// - An Addr is always a single network address
// - An Addr is always fully resolved (no hostnames, no named ports)
class Addr {
 public:
  using Pointer = std::shared_ptr<const AddrImpl>;

  // Addr is constructible from an implementation.
  Addr(Pointer ptr) noexcept : ptr_(std::move(ptr)) {}

  // Addr is default-constructible, copyable, and moveable.
  Addr() noexcept = default;
  Addr(const Addr&) noexcept = default;
  Addr(Addr&&) noexcept = default;
  Addr& operator=(const Addr&) noexcept = default;
  Addr& operator=(Addr&&) noexcept = default;

  // Resets to the default-constructed state.
  void reset() noexcept { ptr_.reset(); }

  // Swaps this Addr with another.
  void swap(Addr& other) noexcept { ptr_.swap(other.ptr_); }

  // Returns true iff this Addr is non-empty.
  explicit operator bool() const noexcept { return !!ptr_; }

  // Returns this Addr's implementation.
  const Pointer& implementation() const noexcept { return ptr_; }
  Pointer& implementation() noexcept { return ptr_; }

  // Returns the protocol name.
  // Examples: "tcp6"; "udp4"; "unixgram"
  std::string protocol() const {
    if (!ptr_) return std::string();
    return ptr_->protocol();
  }

  // Returns the protocol type.
  // Examples: |stream|, |datagram|, |datagram|
  ProtocolType protocol_type() const {
    if (!ptr_) return ProtocolType::unspecified;
    return ptr_->protocol_type();
  }

  // Returns the human-readable address.
  // Examples: "[::1]:80"; "127.0.0.1:22"; "/dev/log"
  std::string address() const {
    if (!ptr_) return std::string();
    return ptr_->address();
  }

  // Returns the IP or other host identifier, if applicable.
  // Examples: "::1"; "127.0.0.1"; ""
  std::string ip() const {
    if (!ptr_) return std::string();
    return ptr_->ip();
  }

  // Returns the port number, if applicable.
  // Examples: 80; 22; 0
  uint16_t port() const {
    if (!ptr_) return 0;
    return ptr_->port();
  }

  // Returns the raw address.
  // - For OS-native protocols, this is a |struct sockaddr|
  std::pair<const void*, std::size_t> raw() const {
    if (!ptr_) return std::make_pair("", 0U);
    return ptr_->raw();
  }

  // Returns |raw()| as a std::string. Convenience method.
  std::string raw_string() const {
    auto pair = raw();
    return std::string(reinterpret_cast<const char*>(pair.first), pair.second);
  }

  // Addr is stringable.
  void append_to(std::string&) const;
  std::string as_string() const;
  friend inline std::ostream& operator<<(std::ostream& o, const Addr& addr) {
    return (o << addr.as_string());
  }

  // Addr is hashable.
  std::size_t hash() const;

 private:
  Pointer ptr_;
};

// Addr is swappable.
inline void swap(Addr& a, Addr& b) noexcept { a.swap(b); }

// Addr is comparable for equality.
bool operator==(const Addr& a, const Addr& b) noexcept;
inline bool operator!=(const Addr& a, const Addr& b) noexcept {
  return !(a == b);
}

}  // namespace net

namespace std {
template <>
struct hash<net::Addr> {
  std::size_t operator()(const net::Addr& addr) const { return addr.hash(); }
};
}  // namespace std

#endif  // NET_ADDR_H
