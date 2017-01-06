// net/ip.h - IPv4 and IPv6 addresses
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_IP_H
#define NET_IP_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <ostream>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/result.h"

namespace net {

// IPClassification matches a given IP address against standards-defined IP
// ranges, to determine which properties the address might have.
class IPClassification {
 private:
  enum {
    ucast_bit = 0x0001,
    // 0x0002 is reserved for anycast
    mcast_bit = 0x0004,
    bcast_bit = 0x0008,

    node_bit = 0x0010,
    link_bit = 0x0020,
    site_bit = 0x0040,
    glbl_bit = 0x0080,

    unspec_bit = 0x0100,
    loop_bit = 0x0200,
    // 0x0400 is reserved for future use
    // 0x0800 is reserved for future use

    // 0x1000 is reserved for future use
    // 0x2000 is reserved for future use
    ipv4_bit = 0x4000,
    ipv6_bit = 0x8000,
  };

  static uint16_t classify(const uint8_t* ptr, std::size_t len) noexcept;

  explicit constexpr IPClassification(uint16_t bits) noexcept : bits_(bits) {}

  constexpr bool has(uint16_t mask) const noexcept {
    return (bits_ & mask) != 0;
  }

 public:
  // IPClassification is default constructible.
  // - This is equivalent to classifying the empty IP address.
  constexpr IPClassification() noexcept : bits_(0) {}

  // Classifies the given IP address. |ptr| points to a |len|-byte buffer
  // containing an IP address; |len| must be 0, 4, or 16.
  IPClassification(const uint8_t* ptr, std::size_t len) noexcept
      : bits_(classify(ptr, len)) {}

  explicit constexpr operator bool() const noexcept { return bits_ != 0; }

  // Returns true iff the IP address has the given address ownership.
  // - At most one of these methods returns true
  constexpr bool is_unicast() const noexcept { return has(ucast_bit); }
  constexpr bool is_multicast() const noexcept { return has(mcast_bit); }
  constexpr bool is_broadcast() const noexcept { return has(bcast_bit); }

  // Returns true iff the IP address has the given address scope.
  // - At most one of these methods returns true
  constexpr bool is_node_local() const noexcept { return has(node_bit); }
  constexpr bool is_link_local() const noexcept { return has(link_bit); }
  constexpr bool is_site_local() const noexcept { return has(site_bit); }
  constexpr bool is_global() const noexcept { return has(glbl_bit); }

  // Returns true iff the IP address is an unspecified address.
  constexpr bool is_unspecified() const noexcept { return has(unspec_bit); }

  // Returns true iff the IP address is a loopback address.
  constexpr bool is_loopback() const noexcept { return has(loop_bit); }

  // Returns true iff the IP address is for the given protocol.
  // - |is_ipv4()| returns true for IPv4 addresses (both 4-byte and 16-byte)
  // - |is_ipv6()| returns true for all other 16-byte addresses
  constexpr bool is_ipv4() const noexcept { return has(ipv4_bit); }
  constexpr bool is_ipv6() const noexcept { return has(ipv6_bit); }

  // Convenience method that returns true iff any of the |is_*_local()| methods
  // would return true.
  constexpr bool is_local() const noexcept {
    return has(node_bit | link_bit | site_bit);
  }

 private:
  uint16_t bits_;
};

// IP represents a single IP address. It holds an array of bytes, which may be
// of length 0, 4, or 16, and which always represents the contained address in
// network order.
class IP {
 public:
  // v4_t is a tag type for IP constructors that yield 4-byte addresses.
  struct v4_t {
    constexpr v4_t() noexcept = default;
  };

  // v6_t is a tag type for IP constructors that yield 16-byte addresses.
  struct v6_t {
    constexpr v6_t() noexcept = default;
  };

  // Concrete instances of the tag types above.
  static constexpr v4_t v4 = {};
  static constexpr v6_t v6 = {};

  // Named constants for the allowed address lengths.
  // - |size() == kEmptyLen| marks an empty IP (neither IPv4 nor IPv6)
  // - |size() == kIPv4Len| marks a 4-byte IPv4 IP
  // - |size() == kIPv6Len| marks a 16-byte IP (usually IPv6, may be IPv4)
  static constexpr uint16_t kEmptyLen = 0;
  static constexpr uint16_t kIPv4Len = 4;
  static constexpr uint16_t kIPv6Len = 16;

  // Well-known IPv4 addresses
  static IP unspecified_v4() noexcept { return IP(v4, 0, 0, 0, 0); }
  static IP localhost_v4() noexcept { return IP(v4, 127, 0, 0, 1); }
  static IP all_systems_v4() noexcept { return IP(v4, 224, 0, 0, 1); }
  static IP all_routers_v4() noexcept { return IP(v4, 224, 0, 0, 2); }
  static IP broadcast_v4() noexcept { return IP(v4, 255, 255, 255, 255); }

  // Well-known IPv6 addresses
  static IP unspecified_v6() noexcept { return IP(v6, 0, 0, 0, 0, 0, 0, 0, 0); }
  static IP localhost_v6() noexcept { return IP(v6, 0, 0, 0, 0, 0, 0, 0, 1); }
  static IP this_node_v6() noexcept {
    return IP(v6, 0xff01, 0, 0, 0, 0, 0, 0, 1);
  }
  static IP all_link_nodes_v6() noexcept {
    return IP(v6, 0xff02, 0, 0, 0, 0, 0, 0, 1);
  }
  static IP all_link_routers_v6() noexcept {
    return IP(v6, 0xff02, 0, 0, 0, 0, 0, 0, 2);
  }
  static IP all_site_routers_v6() noexcept {
    return IP(v6, 0xff05, 0, 0, 0, 0, 0, 0, 2);
  }

  // Parses an IP address from a string.
  static base::Result parse(IP* out, const std::string& str);

 private:
  IP(uint16_t len, uint8_t a, uint8_t b, uint8_t c, uint8_t d)
  noexcept : len_(len) {
    ::bzero(raw_, 10);
    raw_[10] = 0xff;
    raw_[11] = 0xff;
    raw_[12] = a;
    raw_[13] = b;
    raw_[14] = c;
    raw_[15] = d;
    cls_ = IPClassification(data(), size());
  }

  IP(uint16_t len, uint32_t x)
  noexcept
      : IP(len, (x >> 24) & 0xff, (x >> 16) & 0xff, (x >> 8) & 0xff, x & 0xff) {
  }

 public:
  // IP is default constructible.
  IP() noexcept : len_(kEmptyLen), cls_() { ::bzero(raw_, sizeof(raw_)); }

  // IP is copyable and moveable.
  IP(const IP&) noexcept = default;
  IP(IP&&) noexcept = default;
  IP& operator=(const IP&) noexcept = default;
  IP& operator=(IP&&) noexcept = default;

  // Constructs a 4-byte IPv4 address from its bytes.
  IP(v4_t, uint8_t a, uint8_t b, uint8_t c, uint8_t d)
  noexcept : IP(kIPv4Len, a, b, c, d) {}

  // Constructs a 16-byte IPv4 address from its last 4 bytes.
  IP(v6_t, uint8_t a, uint8_t b, uint8_t c, uint8_t d)
  noexcept : IP(kIPv6Len, a, b, c, d) {}

  // Like the above constructors, but takes a 32-bit uint in host byte order.
  IP(v4_t, uint32_t x) noexcept : IP(kIPv4Len, x) {}
  IP(v6_t, uint32_t x) noexcept : IP(kIPv6Len, x) {}

  // Constructs an IPv6 address from eight 16-bit uints in host byte order.
  IP(v6_t, uint16_t p, uint16_t q, uint16_t r, uint16_t s, uint16_t t,
     uint16_t u, uint16_t v, uint16_t w)
  noexcept : len_(kIPv6Len) {
    raw_[0] = (p >> 8);
    raw_[1] = p;
    raw_[2] = (q >> 8);
    raw_[3] = q;
    raw_[4] = (r >> 8);
    raw_[5] = r;
    raw_[6] = (s >> 8);
    raw_[7] = s;
    raw_[8] = (t >> 8);
    raw_[9] = t;
    raw_[10] = (u >> 8);
    raw_[11] = u;
    raw_[12] = (v >> 8);
    raw_[13] = v;
    raw_[14] = (w >> 8);
    raw_[15] = w;
    cls_ = IPClassification(data(), size());
  }

  // Constructs an IP from a begin/end iterator pair for a range of bytes.
  template <typename It>
  IP(It begin, It end) : len_(kEmptyLen), cls_() {
    auto len = std::distance(begin, end);
    switch (len) {
      case kEmptyLen:
        break;

      case kIPv4Len:
        ::bzero(raw_, 10);
        raw_[10] = 0xff;
        raw_[11] = 0xff;
        std::copy(begin, end, raw_ + 12);
        len_ = kIPv4Len;
        break;

      case kIPv6Len:
        std::copy(begin, end, raw_);
        len_ = kIPv6Len;
        break;

      default:
        LOG(DFATAL) << "BUG! Attempt to construct a net::IP with a length of "
                    << len << " bytes; only " << kIPv4Len << "- or " << kIPv6Len
                    << "-byte lengths are legal";
    }
    cls_ = IPClassification(data(), size());
  }

  // Constructs an IP from a byte buffer.
  IP(const uint8_t* ptr, std::size_t len) : IP(CHECK_NOTNULL(ptr), ptr + len) {}

  // Constructs an IP from an explicit list of bytes.
  IP(std::initializer_list<uint8_t> il) : IP(il.begin(), il.end()) {}

  // Returns true iff this IP is non-empty.
  explicit operator bool() const noexcept { return len_ != kEmptyLen; }

  // Returns true iff this IP is empty.
  bool empty() const noexcept { return len_ == kEmptyLen; }

  // Returns true iff this IP contains a 4-byte IPv4 address.
  bool ipv4_len() const noexcept { return len_ == kIPv4Len; }

  // Returns true iff this IP contains a 16-byte address (IPv4 or IPv6).
  bool ipv6_len() const noexcept { return len_ == kIPv6Len; }

  // Returns the classification of this IP address.
  IPClassification classification() const noexcept { return cls_; }

  // Returns true iff this IP address is classified into the named category.
  bool is_unicast() const noexcept { return cls_.is_unicast(); }
  bool is_multicast() const noexcept { return cls_.is_multicast(); }
  bool is_broadcast() const noexcept { return cls_.is_broadcast(); }
  bool is_node_local() const noexcept { return cls_.is_node_local(); }
  bool is_link_local() const noexcept { return cls_.is_link_local(); }
  bool is_site_local() const noexcept { return cls_.is_site_local(); }
  bool is_local() const noexcept { return cls_.is_local(); }
  bool is_global() const noexcept { return cls_.is_global(); }
  bool is_unspecified() const noexcept { return cls_.is_unspecified(); }
  bool is_loopback() const noexcept { return cls_.is_loopback(); }

  // Returns true iff this IP address is of the named type.
  // - |is_ipv4()| returns true for both 4-byte and 16-byte IPv4 addresses
  // - When |is_ipv4()| returns true, |is_ipv6()| necessarily returns false
  // - Both methods return false for empty IP addresses
  bool is_ipv4() const noexcept { return cls_.is_ipv4(); }
  bool is_ipv6() const noexcept { return cls_.is_ipv6(); }

  // Returns the narrowed version of this IP address.
  IP as_narrow() const noexcept {
    IP copy(*this);
    copy.narrow();
    return copy;
  }

  // Returns the widened version of this IP address.
  IP as_wide() const noexcept {
    IP copy(*this);
    copy.widen();
    return copy;
  }

  // Narrows this IP address: converts 16-byte IPv4 addresses to 4-byte.
  void narrow() {
    if (len_ == kIPv6Len && cls_.is_ipv4()) len_ = kIPv4Len;
  }

  // Widens this IP address: converts 4-byte IPv4 addresses to 16-byte.
  void widen() {
    if (len_ == kIPv4Len) len_ = kIPv6Len;
  }

  // Returns a pointer to this IP address's bytes.
  const uint8_t* data() const noexcept { return raw_ + 16 - len_; }

  // Returns the number of bytes in this IP address: 0, 4, or 16.
  std::size_t size() const noexcept { return len_; }

  // Convenience method that returns <data(), size()>.
  std::pair<const uint8_t*, std::size_t> raw() const noexcept {
    return std::make_pair(data(), size());
  }

  // Convenience method that returns this IP address's bytes as a std::string.
  std::string raw_string() const {
    return std::string(reinterpret_cast<const char*>(data()), size());
  }

  // IP is stringable.
  void append_to(std::string* out) const;
  std::string as_string() const;
  friend inline std::ostream& operator<<(std::ostream& o, IP ip) {
    return (o << ip.as_string());
  }

  // IP is hashable.
  std::size_t hash() const noexcept;

 private:
  friend class CIDR;

  uint8_t raw_[16];
  uint16_t len_;
  IPClassification cls_;
};

// IP is comparable for both equality and order.
inline bool operator==(IP a, IP b) noexcept {
  if (a.empty()) return b.empty();
  if (b.empty()) return false;
  a.widen();
  b.widen();
  return ::memcmp(a.data(), b.data(), a.size()) == 0;
}
inline bool operator!=(IP a, IP b) noexcept { return !(a == b); }
inline bool operator<(IP a, IP b) noexcept {
  if (a.empty()) return !b.empty();
  if (b.empty()) return false;
  a.widen();
  b.widen();
  return ::memcmp(a.data(), b.data(), a.size()) < 0;
}
inline bool operator>(IP a, IP b) noexcept { return (b < a); }
inline bool operator<=(IP a, IP b) noexcept { return !(b < a); }
inline bool operator>=(IP a, IP b) noexcept { return !(a < b); }

// CIDR represents a CIDR mask: an IP plus a mask consisting of a string of
// 1-bits followed by a string of 0-bits.  For example:
//
//    "172.16.2.1/12"
//    Base IP: ac 10 02 01  ("172.16.2.1")
//       Mask: ff f0 00 00  (12 1-bits, followed by 20 0-bits)
//   First IP: ac 10 00 00  ("172.16.0.0")
//    Last IP: ac 1f ff ff  ("172.31.255.255")
//
class CIDR {
 public:
  // Parses a CIDR mask from a string.
  static base::Result parse(CIDR* out, const std::string& str);

  // CIDR is default constructible, resulting in an empty CIDR mask.
  CIDR() noexcept : bits_(0) {}

  // CIDR is copyable and moveable.
  CIDR(const CIDR&) noexcept = default;
  CIDR(CIDR&&) noexcept = default;
  CIDR& operator=(const CIDR&) noexcept = default;
  CIDR& operator=(CIDR&&) noexcept = default;

  // CIDR is constructible from an IP and the number of 1-bits in the mask.
  // - If |ip| is empty, then this CIDR mask is empty and |bits| must equal 0
  // - If |ip| is a 4-byte address, then |bits| must not exceed 32
  // - If |ip| is a 16-byte address, then |bits| must not exceed 128
  CIDR(IP ip, unsigned int bits) noexcept;

  // Returns true iff this CIDR mask is non-empty.
  explicit operator bool() const noexcept { return !!ip_; }

  // Returns true iff this CIDR mask is empty.
  bool empty() const noexcept { return !ip_; }

  // Returns the base IP for this CIDR mask.
  IP ip() const noexcept { return ip_; }

  // Returns the number of 1-bits in this CIDR mask.
  unsigned int bits() const noexcept { return bits_; }

  // Returns the first IP in this CIDR mask's range.
  IP first() const noexcept;

  // Returns the last IP in this CIDR mask's range.
  IP last() const noexcept;

  // Returns true iff |ip| is contained in this CIDR mask's range.
  // - The empty CIDR mask contains nothing, not even the empty IP address
  bool contains(IP ip) const noexcept;

  // Returns the narrowed version of this CIDR mask.
  CIDR as_narrow() const noexcept {
    CIDR copy(*this);
    copy.narrow();
    return copy;
  }

  // Returns the widened version of this CIDR mask.
  CIDR as_wide() const noexcept {
    CIDR copy(*this);
    copy.widen();
    return copy;
  }

  // Narrows this CIDR mask: converts 16-byte IPv4 addresses to 4-byte.
  void narrow() {
    if (ip_.size() == IP::kIPv6Len && ip_.is_ipv4()) {
      ip_.len_ = IP::kIPv4Len;
      bits_ -= 96;
    }
  }

  // Widens this CIDR mask: converts 4-byte IPv4 addresses to 16-byte.
  void widen() {
    if (ip_.size() == IP::kIPv4Len) {
      ip_.len_ = IP::kIPv6Len;
      bits_ += 96;
    }
  }

  // CIDR is stringable.
  void append_to(std::string* out) const;
  std::string as_string() const;
  friend inline std::ostream& operator<<(std::ostream& o, CIDR cidr) {
    return (o << cidr.as_string());
  }

  // CIDR is hashable.
  std::size_t hash() const noexcept;

 private:
  IP ip_;
  unsigned int bits_;
};

// CIDR is comparable for both equality and order.
inline bool operator==(CIDR a, CIDR b) noexcept {
  if (a.empty()) return b.empty();
  if (b.empty()) return false;
  a.widen();
  b.widen();
  return a.bits() == b.bits() && a.ip() == b.ip();
}
inline bool operator!=(CIDR a, CIDR b) noexcept { return !(a == b); }
inline bool operator<(CIDR a, CIDR b) noexcept {
  if (a.empty()) return !b.empty();
  if (b.empty()) return false;
  a.widen();
  b.widen();
  return a.bits() < b.bits() || (a.bits() == b.bits() && a.ip() < b.ip());
}
inline bool operator>(CIDR a, CIDR b) noexcept { return (b < a); }
inline bool operator<=(CIDR a, CIDR b) noexcept { return !(b < a); }
inline bool operator>=(CIDR a, CIDR b) noexcept { return !(a < b); }

}  // namespace net

namespace std {
template <>
struct hash<net::IP> {
  std::size_t operator()(net::IP ip) const { return ip.hash(); }
};

template <>
struct hash<net::CIDR> {
  std::size_t operator()(net::CIDR cidr) const { return cidr.hash(); }
};
}  // namespace std

#endif  // NET_IP_H
