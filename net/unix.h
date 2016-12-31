// net/unix.h - Implementation of AF_UNIX network connections
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_UNIX_H
#define NET_UNIX_H

#include "net/protocol.h"

namespace net {

Addr unixaddr(ProtocolType p, const std::string& address);
std::shared_ptr<Protocol> unixprotocol();

}  // namespace net

#endif  // NET_UNIX_H
