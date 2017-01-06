// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/unix.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <map>
#include <mutex>

#include "base/logging.h"
#include "net/addr.h"
#include "net/conn.h"
#include "net/connfd.h"
#include "net/protocol.h"
#include "net/registry.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX (sizeof(sockaddr_un) - sizeof(sa_family_t))
#endif

using P = net::ProtocolType;

namespace net {

namespace {

static const std::map<std::string, ProtocolType>& protomap() {
  static const auto& ref = *new std::map<std::string, ProtocolType>{
      {"unix", P::stream},
      {"unixgram", P::datagram},
      {"unixpacket", P::seqpacket},
  };
  return ref;
}

class UnixAddr : public AddrImpl {
 public:
  UnixAddr(ProtocolType p, const sockaddr_un* ptr, socklen_t len)
      : len_(len), protocol_(p) {
    CHECK_GE(len, sizeof(sa_family_t));
    CHECK_LE(len, sizeof(sockaddr_un));
    CHECK_EQ(ptr->sun_family, AF_UNIX);
    ::memcpy(&sun_, ptr, len);
  }

  std::string protocol() const override {
    switch (protocol_) {
      case P::stream:
        return "unix";
      case P::datagram:
        return "unixgram";
      case P::seqpacket:
        return "unixpacket";
      default:
        LOG(DFATAL) << "BUG! Unknown protocol: " << protocol_;
        return "";
    }
  }

  ProtocolType protocol_type() const noexcept override { return protocol_; }

  std::string address() const override {
    std::string out;
    if (len_ > sizeof(sa_family_t)) {
      const char* ptr = sun_.sun_path;
      std::size_t len = len_ - sizeof(sa_family_t);
      if (*ptr == '\0') {
        out.insert(out.end(), ptr, ptr + len);
        out.front() = '@';
      } else {
        len = ::strnlen(ptr, len);
        out.insert(out.end(), ptr, ptr + len);
      }
    } else {
      // anonymous socket -> ""
    }
    return out;
  }

  std::pair<const void*, std::size_t> raw() const override {
    return std::make_pair(&sun_, len_);
  }

 private:
  sockaddr_un sun_;
  socklen_t len_;
  ProtocolType protocol_;
};

class UnixProtocol : public FDProtocol {
 public:
  bool interprets(int family) const override { return family == AF_UNIX; }

  base::Result interpret(Addr* out, ProtocolType p, const sockaddr* sa,
                         int len) const override;

  bool supports(const std::string& protocol) const override {
    return protomap().count(protocol) != 0;
  }

  base::Result parse(Addr* out, const std::string& protocol,
                     const std::string& address) const override;

  void resolve(event::Task* task, std::vector<Addr>* out,
               const std::string& protocol, const std::string& address,
               const base::Options& options) override;

 private:
  std::shared_ptr<Protocol> self() const override { return unixprotocol(); }

  std::tuple<int, int, int> socket_triple(
      const std::string& protocol) const override;
};

base::Result UnixProtocol::interpret(Addr* out, ProtocolType p,
                                     const sockaddr* sa, int len) const {
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(sa);
  CHECK_GE(len, int(sizeof(sa_family_t)));
  CHECK(interprets(sa->sa_family));
  auto ptr = reinterpret_cast<const sockaddr_un*>(sa);
  *out = Addr(std::make_shared<UnixAddr>(p, ptr, len));
  return base::Result();
}

base::Result UnixProtocol::parse(Addr* out, const std::string& protocol,
                                 const std::string& address) const {
  CHECK_NOTNULL(out);
  CHECK(supports(protocol));
  if (address.size() >= UNIX_PATH_MAX) {
    return base::Result::invalid_argument("AF_UNIX path is too long");
  }
  auto p = protomap().at(protocol);
  *out = unixaddr(p, address);
  return base::Result();
}

void UnixProtocol::resolve(event::Task* task, std::vector<Addr>* out,
                           const std::string& protocol,
                           const std::string& address,
                           const base::Options& options) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  CHECK(supports(protocol));
  if (!task->start()) return;
  Addr addr;
  base::Result r = parse(&addr, protocol, address);
  if (r) out->push_back(std::move(addr));
  task->finish(std::move(r));
}

std::tuple<int, int, int> UnixProtocol::socket_triple(
    const std::string& protocol) const {
  int domain, type, protonum;
  domain = AF_UNIX;
  switch (protomap().at(protocol)) {
    case P::stream:
      type = SOCK_STREAM;
      break;

    case P::datagram:
      type = SOCK_DGRAM;
      break;

    case P::seqpacket:
      type = SOCK_SEQPACKET;
      break;

    default:
      LOG(DFATAL) << "BUG! protocol \"" << protocol << "\" "
                  << "does not map to a known Unix socket type";
      type = SOCK_RAW;
  }
  protonum = 0;
  return std::make_tuple(domain, type, protonum);
}

}  // anonymous namespace

Addr unixaddr(ProtocolType p, const std::string& address) {
  CHECK_LT(address.size(), UNIX_PATH_MAX);
  std::size_t size = address.size();
  if (size >= UNIX_PATH_MAX) size = UNIX_PATH_MAX - 1;

  sockaddr_un sun;
  socklen_t len;
  ::bzero(&sun, sizeof(sun));
  sun.sun_family = AF_UNIX;
  if (address.empty()) {
    len = sizeof(sa_family_t);
  } else if (address.front() == '@') {
    len = sizeof(sa_family_t) + size;
    ::memcpy(sun.sun_path, address.data(), size);
    sun.sun_path[0] = '\0';
  } else {
    len = sizeof(sa_family_t) + size + 1;
    ::memcpy(sun.sun_path, address.c_str(), size + 1);
  }
  return Addr(std::make_shared<UnixAddr>(p, &sun, len));
}

static std::once_flag g_once;
static std::shared_ptr<Protocol>* g_proto = nullptr;

std::shared_ptr<Protocol> unixprotocol() {
  std::call_once(g_once, [] {
    g_proto = new std::shared_ptr<Protocol>(std::make_shared<UnixProtocol>());
  });
  return *g_proto;
}

}  // namespace net

static void init() __attribute__((constructor));
static void init() {
  net::system_registry_mutable().add(nullptr, 50, net::unixprotocol());
}
