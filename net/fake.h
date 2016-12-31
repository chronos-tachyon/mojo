// net/fake.h - Implementation of in-process network connections for tests
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef NET_FAKE_H
#define NET_FAKE_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/util.h"
#include "event/task.h"
#include "net/addr.h"
#include "net/conn.h"
#include "net/protocol.h"

namespace net {

using FakeListenerFn =
    std::function<void(base::Lock&, event::Task*, Conn*, uint32_t, Options)>;

struct FakePortData {
  std::size_t refcount;
  FakeListenerFn listener;
  Addr addr;

  FakePortData() noexcept : refcount(0) {}
};

struct FakeData {
  mutable std::mutex mu;
  std::map<std::string, std::vector<uint32_t>> names;
  std::map<ProtocolType, std::map<uint32_t, FakePortData>> ports;
  std::map<ProtocolType, std::multiset<std::pair<uint32_t, uint32_t>>> pairs;
};

Addr fakeaddr(ProtocolType p, uint32_t x);
Addr fakeaddr(FakeData* data, ProtocolType p, uint32_t x);  // memoized
std::shared_ptr<Protocol> fakeprotocol(FakeData* data);

}  // namespace net

#endif  // NET_FAKE_H
