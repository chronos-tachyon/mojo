// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/sockopt.h"

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#include <cstring>

#include "base/logging.h"

namespace net {

namespace {

struct LevelMapping {
  int level;
  const char* name;
};

static const LevelMapping kSocketLevels[] = {
    {SOL_SOCKET, "SOL_SOCKET"},     {IPPROTO_IP, "IPPROTO_IP"},
    {IPPROTO_IPV6, "IPPROTO_IPV6"}, {IPPROTO_TCP, "IPPROTO_TCP"},
    {IPPROTO_UDP, "IPPROTO_UDP"},   {IPPROTO_ICMP, "IPPROTO_ICMP"},
    {IPPROTO_RAW, "IPPROTO_RAW"},
};

struct OptnameMapping {
  int level;
  int optname;
  const char* name;
};

static const OptnameMapping kSocketOptnames[] = {
    {SOL_SOCKET, SO_ACCEPTCONN, "SO_ACCEPTCONN"},
    {SOL_SOCKET, SO_BINDTODEVICE, "SO_BINDTODEVICE"},
    {SOL_SOCKET, SO_BROADCAST, "SO_BROADCAST"},
    {SOL_SOCKET, SO_BSDCOMPAT, "SO_BSDCOMPAT"},
    {SOL_SOCKET, SO_DEBUG, "SO_DEBUG"},
    {SOL_SOCKET, SO_DOMAIN, "SO_DOMAIN"},
    {SOL_SOCKET, SO_ERROR, "SO_ERROR"},
    {SOL_SOCKET, SO_DONTROUTE, "SO_DONTROUTE"},
    {SOL_SOCKET, SO_KEEPALIVE, "SO_KEEPALIVE"},
    {SOL_SOCKET, SO_LINGER, "SO_LINGER"},
    {SOL_SOCKET, SO_MARK, "SO_MARK"},
    {SOL_SOCKET, SO_OOBINLINE, "SO_OOBINLINE"},
    {SOL_SOCKET, SO_PASSCRED, "SO_PASSCRED"},
    {SOL_SOCKET, SO_PEEK_OFF, "SO_PEEK_OFF"},
    {SOL_SOCKET, SO_PEERCRED, "SO_PEERCRED"},
    {SOL_SOCKET, SO_PRIORITY, "SO_PRIORITY"},
    {SOL_SOCKET, SO_PROTOCOL, "SO_PROTOCOL"},
    {SOL_SOCKET, SO_RCVBUF, "SO_RCVBUF"},
    {SOL_SOCKET, SO_RCVBUFFORCE, "SO_RCVBUFFORCE"},
    {SOL_SOCKET, SO_RCVLOWAT, "SO_RCVLOWAT"},
    {SOL_SOCKET, SO_SNDLOWAT, "SO_SNDLOWAT"},
    {SOL_SOCKET, SO_RCVTIMEO, "SO_RCVTIMEO"},
    {SOL_SOCKET, SO_SNDTIMEO, "SO_SNDTIMEO"},
    {SOL_SOCKET, SO_REUSEADDR, "SO_REUSEADDR"},
    {SOL_SOCKET, SO_RXQ_OVFL, "SO_RXQ_OVFL"},
    {SOL_SOCKET, SO_SNDBUF, "SO_SNDBUF"},
    {SOL_SOCKET, SO_SNDBUFFORCE, "SO_SNDBUFFORCE"},
    {SOL_SOCKET, SO_TIMESTAMP, "SO_TIMESTAMP"},
    {SOL_SOCKET, SO_TYPE, "SO_TYPE"},
    {SOL_SOCKET, SO_BUSY_POLL, "SO_BUSY_POLL"},

    {IPPROTO_IP, IP_ADD_MEMBERSHIP, "IP_ADD_MEMBERSHIP"},
    {IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, "IP_ADD_SOURCE_MEMBERSHIP"},
    {IPPROTO_IP, IP_BLOCK_SOURCE, "IP_BLOCK_SOURCE"},
    {IPPROTO_IP, IP_DROP_MEMBERSHIP, "IP_DROP_MEMBERSHIP"},
    {IPPROTO_IP, IP_FREEBIND, "IP_FREEBIND"},
    {IPPROTO_IP, IP_HDRINCL, "IP_HDRINCL"},
    {IPPROTO_IP, IP_MSFILTER, "IP_MSFILTER"},
    {IPPROTO_IP, IP_MTU, "IP_MTU"},
    {IPPROTO_IP, IP_MTU_DISCOVER, "IP_MTU_DISCOVER"},
    {IPPROTO_IP, IP_MULTICAST_ALL, "IP_MULTICAST_ALL"},
    {IPPROTO_IP, IP_MULTICAST_IF, "IP_MULTICAST_IF"},
    {IPPROTO_IP, IP_MULTICAST_LOOP, "IP_MULTICAST_LOOP"},
    {IPPROTO_IP, IP_MULTICAST_TTL, "IP_MULTICAST_TTL"},
    //{IPPROTO_IP, IP_NODEFRAG, "IP_NODEFRAG"},
    {IPPROTO_IP, IP_OPTIONS, "IP_OPTIONS"},
    {IPPROTO_IP, IP_PKTINFO, "IP_PKTINFO"},
    {IPPROTO_IP, IP_RECVERR, "IP_RECVERR"},
    {IPPROTO_IP, IP_RECVOPTS, "IP_RECVOPTS"},
    {IPPROTO_IP, IP_RECVORIGDSTADDR, "IP_RECVORIGDSTADDR"},
    {IPPROTO_IP, IP_RECVTOS, "IP_RECVTOS"},
    {IPPROTO_IP, IP_RECVTTL, "IP_RECVTTL"},
    {IPPROTO_IP, IP_RETOPTS, "IP_RETOPTS"},
    {IPPROTO_IP, IP_ROUTER_ALERT, "IP_ROUTER_ALERT"},
    {IPPROTO_IP, IP_TOS, "IP_TOS"},
    {IPPROTO_IP, IP_TRANSPARENT, "IP_TRANSPARENT"},
    {IPPROTO_IP, IP_TTL, "IP_TTL"},
    {IPPROTO_IP, IP_UNBLOCK_SOURCE, "IP_UNBLOCK_SOURCE"},

    {IPPROTO_IPV6, IPV6_ADDRFORM, "IPV6_ADDRFORM"},
    {IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, "IPV6_ADD_MEMBERSHIP"},
    {IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, "IPV6_DROP_MEMBERSHIP"},
    {IPPROTO_IPV6, IPV6_MTU, "IPV6_MTU"},
    {IPPROTO_IPV6, IPV6_MTU_DISCOVER, "IPV6_MTU_DISCOVER"},
    {IPPROTO_IPV6, IPV6_MULTICAST_HOPS, "IPV6_MULTICAST_HOPS"},
    {IPPROTO_IPV6, IPV6_MULTICAST_IF, "IPV6_MULTICAST_IF"},
    {IPPROTO_IPV6, IPV6_MULTICAST_LOOP, "IPV6_MULTICAST_LOOP"},
    {IPPROTO_IPV6, IPV6_RECVPKTINFO, "IPV6_RECVPKTINFO"},
    {IPPROTO_IPV6, IPV6_RTHDR, "IPV6_RTHDR"},
    {IPPROTO_IPV6, IPV6_AUTHHDR, "IPV6_AUTHHDR"},
    {IPPROTO_IPV6, IPV6_DSTOPTS, "IPV6_DSTOPTS"},
    {IPPROTO_IPV6, IPV6_HOPOPTS, "IPV6_HOPOPTS"},
    //{IPPROTO_IPV6, IPV6_FLOWINFO, "IPV6_FLOWINFO"},
    {IPPROTO_IPV6, IPV6_HOPLIMIT, "IPV6_HOPLIMIT"},
    {IPPROTO_IPV6, IPV6_RECVERR, "IPV6_RECVERR"},
    {IPPROTO_IPV6, IPV6_ROUTER_ALERT, "IPV6_ROUTER_ALERT"},
    {IPPROTO_IPV6, IPV6_UNICAST_HOPS, "IPV6_UNICAST_HOPS"},
    {IPPROTO_IPV6, IPV6_V6ONLY, "IPV6_V6ONLY"},

    {IPPROTO_TCP, TCP_CONGESTION, "TCP_CONGESTION"},
    {IPPROTO_TCP, TCP_CORK, "TCP_CORK"},
    {IPPROTO_TCP, TCP_DEFER_ACCEPT, "TCP_DEFER_ACCEPT"},
    {IPPROTO_TCP, TCP_INFO, "TCP_INFO"},
    {IPPROTO_TCP, TCP_KEEPCNT, "TCP_KEEPCNT"},
    {IPPROTO_TCP, TCP_KEEPIDLE, "TCP_KEEPIDLE"},
    {IPPROTO_TCP, TCP_KEEPINTVL, "TCP_KEEPINTVL"},
    {IPPROTO_TCP, TCP_LINGER2, "TCP_LINGER2"},
    {IPPROTO_TCP, TCP_MAXSEG, "TCP_MAXSEG"},
    {IPPROTO_TCP, TCP_NODELAY, "TCP_NODELAY"},
    {IPPROTO_TCP, TCP_QUICKACK, "TCP_QUICKACK"},
    {IPPROTO_TCP, TCP_SYNCNT, "TCP_SYNCNT"},
    {IPPROTO_TCP, TCP_USER_TIMEOUT, "TCP_USER_TIMEOUT"},
    {IPPROTO_TCP, TCP_WINDOW_CLAMP, "TCP_WINDOW_CLAMP"},

    {IPPROTO_UDP, UDP_CORK, "UDP_CORK"},

    //{IPPROTO_ICMP, ICMP_FILTER, "ICMP_FILTER"},
    //{IPPROTO_RAW, ICMP_FILTER, "ICMP_FILTER"},
};

}  // anonymous namespace

base::Result SockOpt::get(base::FD fd, void* optval, unsigned int* optlen) const {
  CHECK_NOTNULL(fd);
  auto pair = fd->acquire_fd();
  int rc = ::getsockopt(pair.first, level_, optname_, optval, optlen);
  if (rc != 0) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "getsockopt(2)");
  }
  return base::Result();
}

base::Result SockOpt::set(base::FD fd, const void* optval,
                          unsigned int optlen) const {
  CHECK_NOTNULL(fd);
  auto pair = fd->acquire_fd();
  int rc = ::setsockopt(pair.first, level_, optname_, optval, optlen);
  if (rc != 0) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "setsockopt(2)");
  }
  return base::Result();
}

void SockOpt::append_to(std::string& buffer) const {
  buffer += '<';

  bool found = false;
  for (const auto& mapping : kSocketLevels) {
    if (mapping.level == level_) {
      buffer += mapping.name;
      found = true;
      break;
    }
  }
  if (!found) buffer += '?';

  buffer += ", ";

  found = false;
  for (const auto& mapping : kSocketOptnames) {
    if (mapping.level == level_ && mapping.optname == optname_) {
      buffer += mapping.name;
      found = true;
      break;
    }
  }
  if (!found) buffer += '?';

  buffer += '>';
}

std::string SockOpt::as_string() const {
  std::string out;
  append_to(out);
  return out;
}

const SockOpt sockopt_broadcast = SockOpt(SOL_SOCKET, SO_BROADCAST);
const SockOpt sockopt_error = SockOpt(SOL_SOCKET, SO_ERROR);
const SockOpt sockopt_keepalive = SockOpt(SOL_SOCKET, SO_KEEPALIVE);
const SockOpt sockopt_passcred = SockOpt(SOL_SOCKET, SO_PASSCRED);
const SockOpt sockopt_peercred = SockOpt(SOL_SOCKET, SO_PEERCRED);
const SockOpt sockopt_rcvbuf = SockOpt(SOL_SOCKET, SO_RCVBUF);
const SockOpt sockopt_sndbuf = SockOpt(SOL_SOCKET, SO_SNDBUF);
const SockOpt sockopt_rcvtimeo = SockOpt(SOL_SOCKET, SO_RCVTIMEO);
const SockOpt sockopt_sndtimeo = SockOpt(SOL_SOCKET, SO_SNDTIMEO);
const SockOpt sockopt_reuseaddr = SockOpt(SOL_SOCKET, SO_REUSEADDR);
const SockOpt sockopt_ipv6_v6only = SockOpt(IPPROTO_IPV6, IPV6_V6ONLY);
const SockOpt sockopt_tcp_cork = SockOpt(IPPROTO_TCP, TCP_CORK);
const SockOpt sockopt_tcp_nodelay = SockOpt(IPPROTO_TCP, TCP_NODELAY);
const SockOpt sockopt_udp_cork = SockOpt(IPPROTO_UDP, UDP_CORK);

}  // namespace net
