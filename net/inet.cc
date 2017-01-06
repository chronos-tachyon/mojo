// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/inet.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

#include "base/logging.h"
#include "net/addr.h"
#include "net/conn.h"
#include "net/connfd.h"
#include "net/protocol.h"
#include "net/registry.h"

using P = net::ProtocolType;
using RC = base::ResultCode;

namespace net {

namespace {

static inline const sockaddr_in* RICSIN(const void* ptr) {
  return reinterpret_cast<const sockaddr_in*>(ptr);
}

static inline const sockaddr_in6* RICSIN6(const void* ptr) {
  return reinterpret_cast<const sockaddr_in6*>(ptr);
}

static int socktype_for(net::ProtocolType p) {
  switch (p) {
    case P::raw:
      return SOCK_RAW;
    case P::datagram:
      return SOCK_DGRAM;
    case P::rdm:
      return SOCK_RDM;
    case P::seqpacket:
      return SOCK_SEQPACKET;
    case P::stream:
      return SOCK_STREAM;
    case P::unspecified:
      break;
  }
  LOG(DFATAL) << "BUG! Unknown ProtocolType " << static_cast<uint8_t>(p);
  return SOCK_STREAM;
}

static const std::map<std::string, ProtocolType>& protomap() {
  static const auto& ref = *new std::map<std::string, ProtocolType>{
      {"raw4", P::raw}, {"tcp4", P::stream}, {"udp4", P::datagram},
      {"raw6", P::raw}, {"tcp6", P::stream}, {"udp6", P::datagram},
      {"raw", P::raw},  {"tcp", P::stream},  {"udp", P::datagram},
  };
  return ref;
}

struct GAIError {
  const char* name;
  RC code;
};

static const std::map<int, GAIError>& gaierror_map() {
  static const auto& ref = *new std::map<int, GAIError>{
      {EAI_ADDRFAMILY, {"EAI_ADDRFAMILY", RC::NOT_FOUND}},
      {EAI_AGAIN, {"EAI_AGAIN", RC::UNAVAILABLE}},
      {EAI_BADFLAGS, {"EAI_BADFLAGS", RC::INVALID_ARGUMENT}},
      {EAI_FAIL, {"EAI_FAIL", RC::NOT_FOUND}},
      {EAI_FAMILY, {"EAI_FAMILY", RC::NOT_IMPLEMENTED}},
      {EAI_MEMORY, {"EAI_MEMORY", RC::RESOURCE_EXHAUSTED}},
      {EAI_NODATA, {"EAI_NODATA", RC::NOT_FOUND}},
      {EAI_NONAME, {"EAI_NONAME", RC::NOT_FOUND}},
      {EAI_SERVICE, {"EAI_SERVICE", RC::NOT_FOUND}},
      {EAI_SOCKTYPE, {"EAI_SOCKTYPE", RC::INVALID_ARGUMENT}},
      {EAI_SYSTEM, {"EAI_SYSTEM", RC::UNKNOWN}},
      {EAI_INPROGRESS, {"EAI_INPROGRESS", RC::INTERNAL}},
      {EAI_CANCELED, {"EAI_CANCELED", RC::CANCELLED}},
  };
  return ref;
}

static base::Result result_from_gaierror(int gaierror, int err_no,
                                         const char* what) {
  if (gaierror == 0) {
    return base::Result();
  } else if (gaierror == EAI_SYSTEM) {
    return base::Result::from_errno(err_no, what);
  } else {
    const auto& m = gaierror_map();
    auto it = m.find(gaierror);
    if (it == m.end())
      return base::Result::unknown(what, " gaierror=", gaierror);
    const auto& err = it->second;
    std::string msg;
    base::concat_to(&msg, what, " gaierror=[", err.name, "(", gaierror, "): ",
                    gai_strerror(gaierror), "]");
    return base::Result(err.code, std::move(msg));
  }
}

static void append_port(std::string* out, uint16_t port) {
  if (port == 0) {
    out->push_back('0');
    return;
  }

  std::array<char, 6> buf;
  auto begin = buf.begin();
  auto p = begin;
  while (port != 0) {
    *p = static_cast<unsigned char>(port % 10) + '0';
    ++p;
    port /= 10;
  }
  out->reserve(out->size() + std::distance(begin, p));
  while (p != begin) {
    --p;
    out->push_back(*p);
  }
}

static std::pair<bool, uint8_t> from_dec(char ch) {
  if (ch >= '0' && ch <= '9')
    return std::make_pair(true, ch - '0');
  else
    return std::make_pair(false, 0);
}

template <typename It>
static base::Result parse_port(uint16_t* out, It it, It end) {
  if (it == end) return base::Result::invalid_argument("empty port number");
  unsigned int partial = 0;
  while (it != end) {
    auto pair = from_dec(*it);
    ++it;
    if (!pair.first)
      return base::Result::invalid_argument("named ports are not supported");
    partial = (partial * 10) + pair.second;
    if (partial > 0xffff)
      return base::Result::invalid_argument("port number out of range");
  }
  *out = partial;
  return base::Result();
}

template <typename It>
static base::Result try_parse_port(uint16_t* out, It it, It end) {
  if (it == end) return base::Result();
  if (*it != ':')
    return base::Result::invalid_argument("trailing junk after address");
  ++it;
  return parse_port(out, it, end);
}

class Inet4Addr : public AddrImpl {
 public:
  static Addr make(const sockaddr_in& sin, ProtocolType p) {
    return Addr(std::make_shared<Inet4Addr>(sin, p));
  }

  Inet4Addr(const sockaddr_in& sin, ProtocolType p) : protocol_(p) {
    CHECK_EQ(sin.sin_family, AF_INET);
    ::memcpy(&sin_, &sin, sizeof(sin));
  }

  std::string protocol() const override {
    switch (protocol_) {
      case P::raw:
        return "raw4";
      case P::stream:
        return "tcp4";
      case P::datagram:
        return "udp4";
      default:
        LOG(DFATAL) << "BUG! Unknown protocol: " << protocol_;
        return "";
    }
  }

  ProtocolType protocol_type() const noexcept override { return protocol_; }

  std::string address() const override {
    std::string out;
    out.append(ip());
    out.push_back(':');
    append_port(&out, port());
    return out;
  }

  std::string ip() const override {
    char buf[INET_ADDRSTRLEN];
    ::bzero(buf, sizeof(buf));
    const char* ptr = ::inet_ntop(AF_INET, &sin_.sin_addr, buf, sizeof(buf));
    if (!ptr) {
      int err_no = errno;
      CHECK_OK(base::Result::from_errno(err_no, "inet_ntop(3)"));
    }
    return std::string(ptr, ::strlen(ptr));
  }

  uint16_t port() const override { return ntohs(sin_.sin_port); }

  std::pair<const void*, std::size_t> raw() const override {
    return std::make_pair(&sin_, sizeof(sin_));
  }

 private:
  sockaddr_in sin_;
  ProtocolType protocol_;
};

class Inet6Addr : public AddrImpl {
 public:
  static Addr make(const sockaddr_in6& sin6, ProtocolType p) {
    return Addr(std::make_shared<Inet6Addr>(sin6, p));
  }

  Inet6Addr(const sockaddr_in6& sin6, ProtocolType p) : sin6_(), protocol_(p) {
    CHECK_EQ(sin6.sin6_family, AF_INET6);
    ::memcpy(&sin6_, &sin6, sizeof(sin6));
  }

  std::string protocol() const override {
    switch (protocol_) {
      case P::raw:
        return "raw6";
      case P::stream:
        return "tcp6";
      case P::datagram:
        return "udp6";
      default:
        return "";
    }
  }

  ProtocolType protocol_type() const override { return protocol_; }

  std::string address() const override {
    std::string out;
    out.push_back('[');
    out.append(ip());
    out.push_back(']');
    out.push_back(':');
    append_port(&out, port());
    return out;
  }

  std::string ip() const override {
    char buf[INET6_ADDRSTRLEN];
    ::bzero(buf, sizeof(buf));
    const char* ptr = inet_ntop(AF_INET6, &sin6_.sin6_addr, buf, sizeof(buf));
    if (!ptr) {
      int err_no = errno;
      CHECK_OK(base::Result::from_errno(err_no, "inet_ntop(3)"));
    }
    return std::string(ptr, ::strlen(ptr));
  }

  uint16_t port() const override { return ntohs(sin6_.sin6_port); }

  std::pair<const void*, std::size_t> raw() const override {
    return std::make_pair(&sin6_, sizeof(sin6_));
  }

 private:
  sockaddr_in6 sin6_;
  ProtocolType protocol_;
};

class InetProtocol : public FDProtocol {
 public:
  bool interprets(int family) const override {
    return family == AF_INET || family == AF_INET6;
  }

  base::Result interpret(Addr* out, ProtocolType p, const sockaddr* sa,
                         int len) const override;

  bool supports(const std::string& protocol) const override {
    return protomap().count(protocol) != 0;
  }

  base::Result parse(Addr* out, const std::string& protocol,
                     const std::string& address) const override;

  void resolve(event::Task* task, std::vector<Addr>* out,
               const std::string& protocol, const std::string& address,
               const base::Options& opts) override;

 private:
  std::shared_ptr<Protocol> self() const override { return inetprotocol(); }

  std::tuple<int, int, int> socket_triple(
      const std::string& protocol) const override;
};

base::Result InetProtocol::interpret(Addr* out, ProtocolType p,
                                     const sockaddr* sa, int len) const {
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(sa);
  CHECK_GE(len, int(sizeof(sa_family_t)));
  CHECK(interprets(sa->sa_family));
  switch (sa->sa_family) {
    case AF_INET:
      if (len != sizeof(sockaddr_in)) {
        return base::Result::invalid_argument("wrong length for AF_INET");
      }
      *out = Inet4Addr::make(*RICSIN(sa), p);
      return base::Result();

    case AF_INET6:
      if (len != sizeof(sockaddr_in6)) {
        return base::Result::invalid_argument("wrong length for AF_INET6");
      }
      *out = Inet6Addr::make(*RICSIN6(sa), p);
      return base::Result();

    default:
      return base::Result::not_implemented();
  }
}

base::Result InetProtocol::parse(Addr* out, const std::string& protocol,
                                 const std::string& address) const {
  CHECK_NOTNULL(out);
  CHECK(supports(protocol));
  if (address.empty())
    return base::Result::invalid_argument("empty address not supported");
  if (address.size() > strlen(address.c_str()))
    return base::Result::invalid_argument("addresses are not NUL-safe");

  auto p = protomap().at(protocol);
  bool try_v4 = (protocol.back() != '6');
  bool try_v6 = (protocol.back() != '4');

  std::string host;
  uint16_t port = 0;
  host.reserve(address.size());
  if (try_v6 && address.front() == '[') {
    auto it = address.begin() + 1, end = address.end();
    while (it != end && *it != ']') {
      host.push_back(*it);
      ++it;
    }
    if (it == end)
      return base::Result::invalid_argument("mismatched '[' without ']'");
    ++it;
    auto r = try_parse_port(&port, it, end);
    if (!r) return r;
  } else {
    auto i = address.find_last_of(':');
    if (i != std::string::npos) {
      host.append(address, 0, i);
      auto it = address.begin() + i + 1, end = address.end();
      auto r = parse_port(&port, it, end);
      if (!r) return r;
    }
  }

  if (try_v4) {
    sockaddr_in sin;
    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = ntohs(port);
    int rc = inet_pton(AF_INET, host.c_str(), &sin.sin_addr);
    if (rc == 1) {
      *out = Inet4Addr::make(sin, p);
      return base::Result();
    }
  }

  if (try_v6) {
    sockaddr_in6 sin6;
    bzero(&sin6, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = ntohs(port);
    int rc = inet_pton(AF_INET6, host.c_str(), &sin6.sin6_addr);
    if (rc == 1) {
      *out = Inet6Addr::make(sin6, p);
      return base::Result();
    }
  }

  return base::Result::invalid_argument("failed to parse");
}

static void my_sev_notify_function(union sigval sv);

namespace {
enum class Order : uint8_t {
  untouched = 0,
  ipv4_first = 1,
  ipv6_first = 2,
};

enum class Family : uint8_t {
  favored = 1,
  disfavored = 2,
  unknown = 3,
};

struct AddrStub {
  Family family;
  std::size_t index;

  AddrStub(Family f, std::size_t i) noexcept : family(f), index(i) {}
  friend inline bool operator<(AddrStub a, AddrStub b) noexcept {
    return a.family < b.family || (a.family == b.family && a.index < b.index);
  }
};

struct DontCare {
  Family operator()(int) noexcept { return Family::favored; }
};

struct Favor4 {
  Family operator()(int family) noexcept {
    switch (family) {
      case AF_INET:
        return Family::favored;
      case AF_INET6:
        return Family::disfavored;
      default:
        return Family::unknown;
    }
  }
};

struct Favor6 {
  Family operator()(int family) noexcept {
    switch (family) {
      case AF_INET:
        return Family::disfavored;
      case AF_INET6:
        return Family::favored;
      default:
        return Family::unknown;
    }
  }
};

struct ResolveHelper {
  InetProtocol* const inet;
  event::Task* const task;
  std::vector<Addr>* const out;
  const Order order;
  const ProtocolType p;
  const std::string name;
  const std::string service;
  struct addrinfo hint;
  struct gaicb cb;
  struct sigevent sev;

  ResolveHelper(InetProtocol* inet, event::Task* task, std::vector<Addr>* out,
                Order o, int family, ProtocolType p, std::string n,
                std::string s) noexcept : inet(inet),
                                          task(task),
                                          out(out),
                                          order(o),
                                          p(p),
                                          name(std::move(n)),
                                          service(std::move(s)) {
    ::bzero(&hint, sizeof(hint));
    hint.ai_flags = AI_ADDRCONFIG;
    hint.ai_family = family;
    hint.ai_socktype = socktype_for(p);
    hint.ai_protocol = 0;

    ::bzero(&cb, sizeof(cb));
    if (name.empty()) {
      hint.ai_flags |= AI_PASSIVE;
    } else {
      cb.ar_name = name.c_str();
    }
    cb.ar_service = service.c_str();
    cb.ar_request = &hint;

    ::bzero(&sev, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = my_sev_notify_function;
    sev.sigev_value.sival_ptr = this;
  }

  template <typename Fn>
  void slosh(Fn fn) {
    std::vector<Addr> addrs;
    std::vector<AddrStub> stubs;
    Addr addr;
    base::Result r;

    struct addrinfo* ai = cb.ar_result;
    std::size_t index = 0;
    while (ai != nullptr) {
      r = inet->interpret(&addr, p, ai->ai_addr, ai->ai_addrlen);
      r.expect_ok(__FILE__, __LINE__);
      if (!r) continue;
      addrs.push_back(std::move(addr));
      stubs.emplace_back(fn(ai->ai_addr->sa_family), index);
      ++index;
      ai = ai->ai_next;
    }
    ::freeaddrinfo(cb.ar_result);

    std::sort(stubs.begin(), stubs.end());
    for (const auto& stub : stubs) {
      out->push_back(std::move(addrs[stub.index]));
    }
  }

  void run() {
    int rc = ::gai_error(&cb);
    if (rc != 0) {
      int err_no = errno;
      task->finish(result_from_gaierror(rc, err_no, "getaddrinfo_a(3)"));
      delete this;
      return;
    }

    switch (order) {
      case Order::untouched:
        slosh(DontCare());
        break;

      case Order::ipv4_first:
        slosh(Favor4());
        break;

      case Order::ipv6_first:
        slosh(Favor6());
        break;
    }

    task->finish_ok();
    delete this;
  }
};
}  // anonymous namespace

static void my_sev_notify_function(union sigval sv) {
  auto* helper = reinterpret_cast<ResolveHelper*>(sv.sival_ptr);
  helper->run();
}

void InetProtocol::resolve(event::Task* task, std::vector<Addr>* out,
                           const std::string& protocol,
                           const std::string& address,
                           const base::Options& opts) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  CHECK(supports(protocol));
  if (!task->start()) return;

  auto i = address.rfind(':');
  if (i == std::string::npos) {
    task->finish(base::Result::invalid_argument("missing port"));
    return;
  }
  std::string name = address.substr(0, i);
  std::string service = address.substr(i + 1);
  if (!name.empty() && name.front() == '[' && name.back() == ']') {
    name.pop_back();
    name.erase(name.begin());
  }

  auto p = protomap().at(protocol);
  Order order = Order::untouched;
  int family = AF_UNSPEC;
  switch (protocol.back()) {
    case '4':
      family = AF_INET;
      break;

    case '6':
      family = AF_INET6;
      break;

    default:
      const auto ds = opts.get<net::Options>().dualstack;
      switch (ds) {
        case DualStack::only_ipv4:
          family = AF_INET;
          break;

        case DualStack::only_ipv6:
          family = AF_INET6;
          break;

        case DualStack::prefer_ipv4:
          order = Order::ipv4_first;
          break;

        case DualStack::prefer_ipv6:
          order = Order::ipv6_first;
          break;

        default:
          break;
      }
  }

  auto* helper = new ResolveHelper(this, task, out, order, family, p,
                                   std::move(name), std::move(service));
  struct gaicb* list[1] = {&helper->cb};
  int rc = ::getaddrinfo_a(GAI_NOWAIT, list, 1, &helper->sev);
  if (rc != 0) {
    int err_no = errno;
    task->finish(result_from_gaierror(rc, err_no, "getaddrinfo_a(3)"));
    delete helper;
    return;
  }
}

std::tuple<int, int, int> InetProtocol::socket_triple(
    const std::string& protocol) const {
  int domain, type, protonum;
  switch (protocol.back()) {
    case '4':
      domain = AF_INET;
      break;

    case '6':
      domain = AF_INET6;
      break;

    default:
      LOG(DFATAL) << "BUG! protocol \"" << protocol
                  << "\" does not end in '4' or '6'";
      domain = AF_UNSPEC;
  }
  switch (protomap().at(protocol)) {
    case P::raw:
      type = SOCK_RAW;
      break;

    case P::stream:
      type = SOCK_STREAM;
      break;

    case P::datagram:
      type = SOCK_DGRAM;
      break;

    default:
      LOG(DFATAL) << "BUG! protocol \"" << protocol << "\" "
                  << "does not map to a known IP socket type";
      type = SOCK_RAW;
  }
  protonum = 0;
  return std::make_tuple(domain, type, protonum);
}

}  // anonymous namespace

Addr inetaddr(ProtocolType p, IP ip, uint16_t port) {
  auto raw = ip.raw();
  if (ip.is_ipv4()) {
    sockaddr_in sin;
    ::bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    ::memcpy(&sin.sin_addr.s_addr, raw.first, raw.second);
    return Inet4Addr::make(sin, p);
  } else {
    sockaddr_in6 sin6;
    ::bzero(&sin6, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(port);
    ::memcpy(sin6.sin6_addr.s6_addr, raw.first, raw.second);
    return Inet6Addr::make(sin6, p);
  }
}

static std::once_flag g_once;
static std::shared_ptr<Protocol>* g_proto = nullptr;

std::shared_ptr<Protocol> inetprotocol() {
  std::call_once(g_once, [] {
    g_proto = new std::shared_ptr<Protocol>(std::make_shared<InetProtocol>());
  });
  return *g_proto;
}

}  // namespace net

static void init() __attribute__((constructor));
static void init() {
  net::system_registry_mutable().add(nullptr, 50, net::inetprotocol());
}
