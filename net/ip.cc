// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/ip.h"

#include <arpa/inet.h>

#include "base/logging.h"
#include "net/internal.h"

namespace {
static std::pair<bool, uint8_t> from_dec(char ch) {
  if (ch >= '0' && ch <= '9')
    return std::make_pair(true, ch - '0');
  else
    return std::make_pair(false, 0);
}

struct U128 {
  uint64_t lo;
  uint64_t hi;

  constexpr U128() noexcept : lo(0), hi(0) {}
  constexpr U128(uint64_t hi, uint64_t lo) noexcept : lo(lo), hi(hi) {}
  constexpr U128(uint32_t a, uint32_t b, uint32_t c, uint32_t d) noexcept
      : lo((uint64_t(c) << 32) | uint64_t(d)),
        hi((uint64_t(a) << 32) | uint64_t(b)) {}

  constexpr U128 operator&(U128 rhs) const noexcept {
    return U128(hi & rhs.hi, lo & rhs.lo);
  }
  constexpr U128 operator|(U128 rhs) const noexcept {
    return U128(hi | rhs.hi, lo | rhs.lo);
  }

  friend constexpr bool operator==(U128 lhs, U128 rhs) noexcept {
    return lhs.lo == rhs.lo && lhs.hi == rhs.hi;
  }
};

struct v4rule {
  uint32_t mask;
  uint32_t equal;
  uint16_t bits;
};

struct v6rule {
  U128 mask;
  U128 equal;
  uint16_t bits;
};

static const v4rule v4rules[] = {
    // 0.0.0.0/32 -- IPv4 unspecified
    {0xffffffffU, 0x00000000U, 0x4100},

    // 255.255.255.255/32 -- IPv4 broadcast
    {0xffffffffU, 0xffffffffU, 0x4028},

    // 224.0.0/24 -- IPv4 local subnet multicast
    {0xffffff00U, 0xe0000000U, 0x4024},

    // 169.254/16 -- IPv4 RFC 3927 ad-hoc addressing block
    {0xffff0000U, 0xa9fe0000U, 0x4021},

    // 192.168/16 -- IPv4 RFC 1918 private block
    {0xffff0000U, 0xc0a80000U, 0x4041},

    // 172.16/12 -- IPv4 RFC 1918 private block
    {0xfff00000U, 0xac100000U, 0x4041},

    // 0/8 -- IPv4 reserved
    {0xff000000U, 0x00000000U, 0x4000},

    // 10/8 -- IPv4 RFC 1918 private block
    {0xff000000U, 0x0a000000U, 0x4041},

    // 127/8 -- IPv4 loopback network
    {0xff000000U, 0x7f000000U, 0x4211},

    // 239/8 -- IPv4 admin-scoped multicast
    {0xff000000U, 0xef000000U, 0x4044},

    // 224/4 -- IPv4 "Class D" multicast block
    {0xf0000000U, 0xe0000000U, 0x4084},

    // 240/4 -- IPv4 "Class E" reserved block
    {0xf0000000U, 0xf0000000U, 0x4000},

    // 0/0 -- IPv4 not otherwise matched
    {0x00000000U, 0x00000000U, 0x4081},
};

static const v6rule v6rules[] = {
    // ::/128 -- IPv6 unspecified
    {{0xffffffffU, 0xffffffffU, 0xffffffffU, 0xffffffffU},
     {0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     0x8100},

    // ::1/128 -- IPv6 loopback
    {{0xffffffffU, 0xffffffffU, 0xffffffffU, 0xffffffffU},
     {0x00000000U, 0x00000000U, 0x00000000U, 0x00000001U},
     0x8211},

    // ffx1::/16 -- IPv6 node-local multicast
    {{0xff0f0000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0xff010000U, 0x00000000U, 0x00000000U, 0x00000000U},
     0x8014},

    // ffx2::/16 -- IPv6 link-local multicast
    {{0xff0f0000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0xff020000U, 0x00000000U, 0x00000000U, 0x00000000U},
     0x8024},

    // ffx5::/16 -- IPv6 site-local multicast
    {{0xff0f0000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0xff050000U, 0x00000000U, 0x00000000U, 0x00000000U},
     0x8044},

    // ffxe::/16 -- IPv6 global multicast
    {{0xff0f0000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0xff0e0000U, 0x00000000U, 0x00000000U, 0x00000000U},
     0x8084},

    // fe80::/10 -- IPv6 link-local unicast
    {{0xffc00000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0xfe800000U, 0x00000000U, 0x00000000U, 0x00000000U},
     0x8021},

    // ff00::/8 -- IPv6 multicast not otherwise matched
    {{0xff000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0xff000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     0x8004},

    // fc00::/7 -- IPv6 site-local unicast
    {{0xfe000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0xfc000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     0x8041},

    // 2000::/3 -- IPv6 global unicast
    {{0xe0000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0x20000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     0x8081},

    // ::/0 -- IPv6 not otherwise matched
    {{0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     0x8000},
};

static uint32_t make_u32(const uint8_t* ptr) {
  return (uint32_t(ptr[0]) << 24) | (uint32_t(ptr[1]) << 16) |
         (uint32_t(ptr[2]) << 8) | uint32_t(ptr[3]);
}
}  // anonymous namespace

namespace net {

uint16_t IPClassification::classify(const uint8_t* ptr,
                                    std::size_t len) noexcept {
  if (len == 16) {
    uint32_t a = make_u32(ptr);
    uint32_t b = make_u32(ptr + 4);
    uint32_t c = make_u32(ptr + 8);
    if (a == 0 && b == 0 && c == 0xffffU) {
      ptr += 12;
      len -= 12;
      goto v4;
    }
    uint32_t d = make_u32(ptr + 12);
    U128 x(a, b, c, d);
    const v6rule* rule = v6rules;
    while (true) {
      if ((x & rule->mask) == rule->equal) return rule->bits;
      ++rule;
    }
  }
v4:
  if (len == 4) {
    uint32_t x = make_u32(ptr);
    const v4rule* rule = v4rules;
    while (true) {
      if ((x & rule->mask) == rule->equal) return rule->bits;
      ++rule;
    }
  }
  return 0;
}

constexpr IP::v4_t IP::v4;
constexpr IP::v6_t IP::v6;

constexpr uint16_t IP::kEmptyLen;
constexpr uint16_t IP::kIPv4Len;
constexpr uint16_t IP::kIPv6Len;

base::Result IP::parse(IP* out, const std::string& str) {
  uint8_t raw[16];
  CHECK_NOTNULL(out);
  int rc = ::inet_pton(AF_INET, str.c_str(), raw);
  if (rc == 1) {
    *out = IP(raw, 4);
    return base::Result();
  }
  rc = ::inet_pton(AF_INET6, str.c_str(), raw);
  if (rc == 1) {
    *out = IP(raw, 16);
    return base::Result();
  }
  if (rc == 0) {
    *out = IP();
    return base::Result::invalid_argument("failed to parse IP \"", str, "\"");
  }
  int err_no = errno;
  *out = IP();
  return base::Result::from_errno(err_no, "inet_pton(3)");
}

void IP::append_to(std::string* out) const {
  char buf[INET6_ADDRSTRLEN];
  int domain;

  switch (len_) {
    case kEmptyLen:
      return;

    case kIPv4Len:
      domain = AF_INET;
      break;

    case kIPv6Len:
      domain = AF_INET6;
      break;

    default:
      out->append("<error>");
      return;
  }
  const char* str = ::inet_ntop(domain, data(), buf, sizeof(buf));
  if (!str) {
    int err_no = errno;
    base::Result r = base::Result::from_errno(err_no, "inet_ntop(3)");
    r.expect_ok(__FILE__, __LINE__);
    out->append("<error>");
    return;
  }
  out->append(str);
}

std::string IP::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

std::size_t IP::hash() const noexcept {
  return net::internal::hash(raw_, sizeof(raw_));
}

static std::size_t cidr_max_bits(unsigned int iplen) {
  switch (iplen) {
    case IP::kIPv4Len:
      return 32;
    case IP::kIPv6Len:
      return 128;
    default:
      return 0;
  }
}

static void cidr_build_mask(uint8_t* ptr, unsigned int bits, unsigned int iplen) {
  DCHECK_NOTNULL(ptr);
  DCHECK_LE(iplen, 16U);
  DCHECK_LE(bits, cidr_max_bits(iplen));
  uint8_t* end = ptr + 16;
  uint8_t* start = end - iplen;
  while (ptr < start) {
    *ptr = 0xff;
    ++ptr;
  }
  while (bits >= 8) {
    *ptr = 0xff;
    ++ptr;
    bits -= 8;
  }
  if (bits > 0) {
    *ptr = ~(0xffU >> bits);
    ++ptr;
  }
  while (ptr != end) {
    *ptr = 0;
    ++ptr;
  }
}

base::Result CIDR::parse(CIDR* out, const std::string& str) {
  CHECK_NOTNULL(out);
  *out = CIDR();

  const char* begin = str.data();
  const char* end = begin + str.size();
  const char* ptr = end;
  bool found = false;
  while (ptr != begin) {
    --ptr;
    if (*ptr == '/') {
      found = true;
      break;
    }
  }
  if (!found) {
    return base::Result::invalid_argument("missing '/'");
  }

  std::string sub(str.substr(0, ptr - begin));
  unsigned int bits = 0;
  ++ptr;
  while (ptr != end) {
    auto pair = from_dec(*ptr);
    ++ptr;
    if (!pair.first)
      return base::Result::invalid_argument("CIDR mask is not a number");
    bits = (bits * 10) + pair.second;
    if (bits > 128)
      return base::Result::invalid_argument("CIDR mask is out of range");
  }

  net::IP ip;
  base::Result r = IP::parse(&ip, sub);
  if (r) {
    if (bits > cidr_max_bits(ip.size()))
      return base::Result::invalid_argument("CIDR mask is out of range");
    *out = CIDR(ip, bits);
  }
  return r;
}

CIDR::CIDR(IP ip, unsigned int bits) noexcept : ip_(ip), bits_(bits) {
  std::size_t max = cidr_max_bits(ip.size());
  CHECK_LE(bits_, max);
  if (bits_ > max) bits_ = max;
  uint8_t mask[16];
  cidr_build_mask(mask, bits_, ip_.size());
  for (std::size_t i = 0; i < 16; ++i) {
    ip_.raw_[i] &= mask[i];
  }
}

IP CIDR::first() const noexcept {
  return ip_;
}

IP CIDR::last() const noexcept {
  IP copy(ip_);
  uint8_t mask[16];
  cidr_build_mask(mask, bits_, ip_.size());
  for (std::size_t i = 0; i < 16; ++i) {
    copy.raw_[i] &= mask[i];
    copy.raw_[i] |= (0xffU & ~mask[i]);
  }
  return copy;
}

bool CIDR::contains(IP ip) const noexcept {
  if (ip_.size() == 0 || ip.size() == 0) return false;
  uint8_t mask[16];
  cidr_build_mask(mask, bits_, ip_.size());
  for (std::size_t i = 0; i < 16; ++i) {
    if ((ip_.raw_[i] & mask[i]) != (ip.raw_[i] & mask[i])) return false;
  }
  return true;
}

void CIDR::append_to(std::string* out) const {
  if (!ip_) return;
  base::concat_to(out, ip_, '/', bits_);
}

std::string CIDR::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

std::size_t CIDR::hash() const noexcept {
  std::size_t bits = bits_;
  if (ip_.size() == IP::kIPv4Len) bits += 96;
  return net::internal::mix(ip_.hash(), bits);
}

}  // namespace net
