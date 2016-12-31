// net/testing.h - Helpers for writing network protocol unit tests
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_TESTING_H
#define NET_TESTING_H

#include "net/protocol.h"

namespace net {

void TestListenAndDial(std::shared_ptr<Protocol> p, Addr addr);

}  // namespace net

#endif  // NET_TESTING_H
