// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/addr.h"

#include <cstring>

#include "base/logging.h"
#include "net/internal.h"

namespace net {

static const char* const kProtocolTypeNames[] = {
    "(invalid)", "raw", "datagram", "rdm", "seqpacket", "stream",
};

void append_to(std::string& out, ProtocolType p) {
  out.append(kProtocolTypeNames[static_cast<uint8_t>(p)]);
}

void Addr::append_to(std::string& out) const {
  base::concat_to(out, protocol(), "://", address());
}

std::string Addr::as_string() const {
  std::string out;
  append_to(out);
  return out;
}

std::size_t Addr::hash() const {
  auto pair = raw();
  return net::internal::hash(pair.first, pair.second);
}

bool operator==(const Addr& a, const Addr& b) noexcept {
  std::string ap = a.protocol();
  std::string bp = b.protocol();
  if (ap != bp) return false;

  const void *aptr, *bptr;
  std::size_t alen, blen;
  std::tie(aptr, alen) = a.raw();
  std::tie(bptr, blen) = b.raw();
  return alen == blen && ::memcmp(aptr, bptr, blen) == 0;
}

}  // namespace net
