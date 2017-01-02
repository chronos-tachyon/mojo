// net/sockopt.h - A wrapper around socket options
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_SOCKOPT_H
#define NET_SOCKOPT_H

#include <string>

#include "base/fd.h"
#include "base/result.h"

namespace net {

// Represents a socket option.  See setsockopt(2) for background.
//
// Compared to raw numbers, the use of this class provides two small benefits:
// - Encapsulates knowledge of how the numbers are obtained
// - Provides better descriptions of the numbers in error messages
//
class SockOpt {
 public:
  constexpr SockOpt(int level, int optname) noexcept : level_(level),
                                                       optname_(optname) {}
  constexpr SockOpt(const SockOpt&) noexcept = default;
  SockOpt& operator=(const SockOpt&) noexcept = default;

  constexpr int level() const noexcept { return level_; }
  constexpr int optname() const noexcept { return optname_; }

  base::Result get(base::FD fd, void* optval, unsigned int* optlen) const;
  base::Result set(base::FD fd, const void* optval, unsigned int optlen) const;

  void append_to(std::string* buffer) const;
  std::string as_string() const;
  operator std::string() const { return as_string(); }

 private:
  int level_;
  int optname_;
};

inline constexpr bool operator==(SockOpt a, SockOpt b) noexcept {
  return a.level() == b.level() && a.optname() == b.optname();
}
inline constexpr bool operator!=(SockOpt a, SockOpt b) noexcept {
  return !(a == b);
}

inline constexpr bool operator<(SockOpt a, SockOpt b) noexcept {
  return a.level() < b.level() ||
         (a.level() == b.level() && a.optname() < b.optname());
}
inline constexpr bool operator>(SockOpt a, SockOpt b) noexcept {
  return (b < a);
}
inline constexpr bool operator>=(SockOpt a, SockOpt b) noexcept {
  return !(a < b);
}
inline constexpr bool operator<=(SockOpt a, SockOpt b) noexcept {
  return !(b < a);
}

extern const SockOpt sockopt_broadcast;
extern const SockOpt sockopt_error;
extern const SockOpt sockopt_keepalive;
extern const SockOpt sockopt_passcred;
extern const SockOpt sockopt_peercred;
extern const SockOpt sockopt_rcvbuf;
extern const SockOpt sockopt_sndbuf;
extern const SockOpt sockopt_rcvtimeo;
extern const SockOpt sockopt_sndtimeo;
extern const SockOpt sockopt_reuseaddr;
extern const SockOpt sockopt_ipv6_v6only;
extern const SockOpt sockopt_tcp_cork;
extern const SockOpt sockopt_tcp_nodelay;
extern const SockOpt sockopt_udp_cork;

}  // namespace net

#endif  // NET_SOCKOPT_H
