// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <netinet/in.h>
#include <sys/socket.h>

#include "base/logging.h"
#include "base/result_testing.h"
#include "event/manager.h"
#include "event/task.h"
#include "net/addr.h"
#include "net/inet.h"
#include "net/protocol.h"
#include "net/registry.h"
#include "net/testing.h"

using P = net::ProtocolType;
using RC = base::Result::Code;

static const sockaddr* RICSA(const void* ptr) {
  return reinterpret_cast<const sockaddr*>(ptr);
}

TEST(Linker, Init) {
  EXPECT_TRUE(net::system_registry().supports("tcp"));
  EXPECT_TRUE(net::system_registry().supports("udp4"));
  EXPECT_TRUE(net::system_registry().supports("raw6"));
}

TEST(InetProtocol, InterpretAndStringify4) {
  sockaddr_in sin;
  ::bzero(&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(12345);
  sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  auto p = net::inetprotocol();
  net::Addr addr;
  ASSERT_OK(p->interpret(&addr, P::stream, RICSA(&sin), sizeof(sin)));
  EXPECT_EQ("tcp4", addr.protocol());
  EXPECT_EQ("127.0.0.1:12345", addr.address());
  EXPECT_EQ("127.0.0.1", addr.ip());
  EXPECT_EQ(12345U, addr.port());
}

TEST(InetProtocol, InterpretAndStringify6) {
  sockaddr_in6 sin6;
  ::bzero(&sin6, sizeof(sin6));
  sin6.sin6_family = AF_INET6;
  sin6.sin6_port = htons(12345);
  sin6.sin6_addr = IN6ADDR_LOOPBACK_INIT;

  auto p = net::inetprotocol();
  net::Addr addr;
  ASSERT_OK(p->interpret(&addr, P::datagram, RICSA(&sin6), sizeof(sin6)));
  EXPECT_EQ("udp6", addr.protocol());
  EXPECT_EQ("[::1]:12345", addr.address());
  EXPECT_EQ("::1", addr.ip());
  EXPECT_EQ(12345U, addr.port());
}

TEST(InetProtocol, Parse4) {
  auto p = net::inetprotocol();
  net::Addr out;
  ASSERT_OK(p->parse(&out, "tcp", "127.0.0.1:80"));
  EXPECT_EQ("tcp4", out.protocol());
  EXPECT_EQ(P::stream, out.protocol_type());
  EXPECT_EQ("127.0.0.1:80", out.address());
  EXPECT_EQ("127.0.0.1", out.ip());
  EXPECT_EQ(80U, out.port());
  auto raw = out.raw();
  const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(raw.first);
  ASSERT_EQ(sizeof(*sin), raw.second);
  EXPECT_EQ(AF_INET, sin->sin_family);
  EXPECT_EQ(80U, ntohs(sin->sin_port));
  EXPECT_EQ(0x7f000001U, ntohl(sin->sin_addr.s_addr));
}

TEST(InetProtocol, Parse6) {
  auto p = net::inetprotocol();
  net::Addr out;
  ASSERT_OK(p->parse(&out, "udp", "[::1]:4000"));
  EXPECT_EQ("udp6", out.protocol());
  EXPECT_EQ("[::1]:4000", out.address());
  EXPECT_EQ("::1", out.ip());
  EXPECT_EQ(4000, out.port());
  auto raw = out.raw();
  const sockaddr_in6* sin6 = reinterpret_cast<const sockaddr_in6*>(raw.first);
  ASSERT_EQ(sizeof(*sin6), raw.second);
  EXPECT_EQ(AF_INET6, sin6->sin6_family);
  EXPECT_EQ(4000, ntohs(sin6->sin6_port));
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[0]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[1]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[2]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[3]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[4]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[5]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[6]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[7]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[8]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[9]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[10]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[11]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[12]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[13]);
  EXPECT_EQ(0U, sin6->sin6_addr.s6_addr[14]);
  EXPECT_EQ(1U, sin6->sin6_addr.s6_addr[15]);
}

TEST(InetProtocol, ListenAndDial) {
  auto p = net::inetprotocol();
  net::Addr addr = net::inetaddr(P::stream, net::IP::localhost_v4(), 0);
  TestListenAndDial(p, addr);
}
