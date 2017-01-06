// net/options.h - Configurable knobs for network behavior
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_OPTIONS_H
#define NET_OPTIONS_H

#include <cstdint>

#include "base/options.h"

namespace net {

enum class DualStack : uint8_t {
  only_ipv4 = 0,
  prefer_ipv4 = 1,
  smart = 2,
  prefer_ipv6 = 3,
  only_ipv6 = 4,
};

enum class DualListen : uint8_t {
  system_default = 0,
  v6mapped = 1,
  v6only = 2,
};

// Options represents knobs that can be tweaked for network connections.
struct Options : public base::OptionsType {
  // Accesses the knob for IPv4/IPv6 dual-stack connect behavior.
  //
  // This selects dial behavior on IPv4/IPv6 dual-stack systems.
  //
  // dualstack = net::DualStack::only_ipv4:
  //    Ignore IPv6 addresses entirely.
  //
  // dualstack = net::DualStack::prefer_ipv4:
  //    Try IPv4 addresses first, fall back on IPv6.
  //
  // dualstack = net::DualStack::smart:
  //    DEFAULT.  Trust getaddrinfo(3) to implement RFC 6724.
  //
  // dualstack = net::DualStack::prefer_ipv6:
  //    Try IPv6 addresses first, fall back on IPv4.
  //
  // dualstack = net::DualStack::only_ipv6:
  //    Ignore IPv4 addresses entirely.
  //
  DualStack dualstack;

  // Accesses the knob for <IPPROTO_IPV6, IPV6_V6ONLY>.
  //
  // This selects listen behavior on dual-stack systems.
  //
  // Specifically, IPv6 listen sockets bound to the unspecified address can
  // optionally be made to accept IPv4 peer connections as well, but this
  // behavior can be changed on a socket-by-socket basis.
  //
  // duallisten = net::DualListen::system_default:
  //    DEFAULT.  Let the system decide.
  //    On Linux, this depends on "sysctl net.ipv6.bindv6only".
  //
  // duallisten = net::DualListen::v6mapped:
  //    Listen sockets that bind to IPv6 "::" are
  //    forced to bind to IPv4 "0.0.0.0" as well.
  //
  // duallisten = net::DualListen::v6only:
  //    Listen sockets that bind to IPv6 "::" are
  //    prevented from binding to IPv4 "0.0.0.0".
  //
  DualListen duallisten;

  // Accesses the knob for <SOL_SOCKET, SO_REUSEADDR>.
  //
  // This relaxes the rules for listen sockets, allowing them to bind to a port
  // that is already in use by connection sockets.  Useful for servers that may
  // need to restart while child connections still exist, or before their TCP
  // wait states complete.
  //
  // DEFAULT: true.
  //
  bool reuseaddr;

  // Options is default constructible.
  Options() noexcept : dualstack(DualStack::smart),
                       duallisten(DualListen::system_default),
                       reuseaddr(true) {}

  // Options is copyable and moveable.
  Options(const Options&) noexcept = default;
  Options(Options&&) noexcept = default;
  Options& operator=(const Options&) noexcept = default;
  Options& operator=(Options&&) noexcept = default;

  // Resets this net::Options to the default values.
  void reset() { *this = Options(); }
};

}  // namespace net

#endif  // NET_OPTIONS_H
