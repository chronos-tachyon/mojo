// net/inet.h - Implementation of IPv4 and IPv6 network connections
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_INET_H
#define NET_INET_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <iterator>

#include "base/logging.h"
#include "net/ip.h"
#include "net/protocol.h"

namespace net {

Addr inetaddr(ProtocolType p, IP ip, uint16_t port);
std::shared_ptr<Protocol> inetprotocol();

}  // namespace net

#endif  // NET_INET_H
