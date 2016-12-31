// net/options.h - Configurable knobs for network behavior
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_OPTIONS_H
#define NET_OPTIONS_H

#include <cstdint>

#include "io/options.h"

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
class Options {
 private:
  enum {
    io_bit = (1U << 0),
  };

  bool has(uint8_t bit) const noexcept { return (has_ & bit) != 0; }

 public:
  // Options is default constructible.
  Options() noexcept : ds_(DualStack::smart),
                       dl_(DualListen::system_default),
                       ra_(true) {}

  // Options is copyable and moveable.
  Options(const Options&) noexcept = default;
  Options(Options&&) noexcept = default;
  Options& operator=(const Options&) noexcept = default;
  Options& operator=(Options&&) noexcept = default;

  // Resets this net::Options to the default values.
  void reset() {
    has_ = 0;
    io_.reset();
    ds_ = DualStack::smart;
    dl_ = DualListen::system_default;
    ra_ = false;
  }

  // Accesses the options for I/O operations.
  const io::Options& io() const noexcept { return io_; }
  io::Options& io() noexcept { return io_; }
  void reset_io() noexcept { io_.reset(); }
  void set_io(io::Options o) noexcept { io_ = std::move(o); }

  // Accesses the knob for IPv4/IPv6 dual-stack connect behavior.
  //
  // This selects dial behavior on IPv4/IPv6 dual-stack systems.
  //
  // reset_dualstack():
  //    Same as set_dualstack(net::DualStack::smart).
  //
  // set_dualstack(net::DualStack::only_ipv4):
  //    Ignore IPv6 addresses entirely.
  //
  // set_dualstack(net::DualStack::prefer_ipv4):
  //    Try IPv4 addresses first, fall back on IPv6.
  //
  // set_dualstack(net::DualStack::smart):
  //    Trust getaddrinfo(3) to implement RFC 6724.
  //
  // set_dualstack(net::DualStack::prefer_ipv6):
  //    Try IPv6 addresses first, fall back on IPv4.
  //
  // set_dualstack(net::DualStack::only_ipv6):
  //    Ignore IPv4 addresses entirely.
  //
  DualStack dualstack() const noexcept { return ds_; }
  void reset_dualstack() noexcept { ds_ = DualStack::smart; }
  void set_dualstack(DualStack ds) noexcept { ds_ = ds; }

  // Accesses the knob for <IPPROTO_IPV6, IPV6_V6ONLY>.
  //
  // This selects listen behavior on dual-stack systems.
  //
  // Specifically, IPv6 listen sockets bound to the unspecified address can
  // optionally be made to accept IPv4 peer connections as well, but this
  // behavior can be changed on a socket-by-socket basis.
  //
  // reset_duallisten():
  //    Same as set_duallisten(net::DualListen::system_default).
  //
  // set_duallisten(net::DualListen::system_default):
  //    Let the system decide.
  //    On Linux, this depends on "sysctl net.ipv6.bindv6only".
  //
  // set_duallisten(net::DualListen::v6mapped):
  //    Listen sockets that bind to IPv6 "::" are
  //    forced to bind to IPv4 "0.0.0.0" as well.
  //
  // set_duallisten(net::DualListen::v6only):
  //    Listen sockets that bind to IPv6 "::" are
  //    prevented from binding to IPv4 "0.0.0.0".
  //
  DualListen duallisten() const noexcept { return dl_; }
  void reset_duallisten() noexcept { dl_ = DualListen::system_default; }
  void set_duallisten(DualListen value) noexcept { dl_ = value; }

  // Accesses the knob for <SOL_SOCKET, SO_REUSEADDR>.
  //
  // This relaxes the rules for listen sockets, allowing them to bind to a port
  // that is already in use by connection sockets.  Useful for servers that may
  // need to restart while child connections still exist, or before their TCP
  // wait states complete.
  //
  bool reuseaddr() const noexcept { return ra_; }
  void reset_reuseaddr() noexcept { ra_ = false; }
  void set_reuseaddr(bool value = true) noexcept { ra_ = value; }

 private:
  io::Options io_;
  DualStack ds_;
  DualListen dl_;
  bool ra_;
  uint8_t has_;
};

Options default_options() noexcept;
void set_default_options(Options opts) noexcept;

}  // namespace net

#endif  // NET_OPTIONS_H
