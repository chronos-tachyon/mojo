// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <sys/socket.h>
#include <sys/un.h>

#include "base/logging.h"
#include "base/result_testing.h"
#include "net/addr.h"
#include "net/registry.h"
#include "net/testing.h"
#include "net/unix.h"

using P = net::ProtocolType;

static const sockaddr* RICSA(const void* ptr) {
  return reinterpret_cast<const sockaddr*>(ptr);
}

static const sockaddr_un* RICSUN(const void* ptr) {
  return reinterpret_cast<const sockaddr_un*>(ptr);
}

TEST(Linker, Init) {
  EXPECT_TRUE(net::system_registry().supports("unix"));
  EXPECT_TRUE(net::system_registry().supports("unixgram"));
  EXPECT_TRUE(net::system_registry().supports("unixpacket"));
}

TEST(UnixProtocol, InterpretAndStringify) {
  auto p = net::unixprotocol();

  sockaddr_un sun;
  ::bzero(&sun, sizeof(sun));
  sun.sun_family = AF_UNIX;

  net::Addr addr;
  base::Result r =
      p->interpret(&addr, P::stream, RICSA(&sun), sizeof(sa_family_t));
  EXPECT_OK(r);
  if (r) {
    EXPECT_EQ("unix", addr.protocol());
    EXPECT_EQ("", addr.address());
  }

  ::memcpy(sun.sun_path, "/foo/bar", 9);
  r = p->interpret(&addr, P::datagram, RICSA(&sun), sizeof(sa_family_t) + 9);
  EXPECT_OK(r);
  if (r) {
    EXPECT_EQ("unixgram", addr.protocol());
    EXPECT_EQ("/foo/bar", addr.address());
  }

  sun.sun_path[0] = 0;
  r = p->interpret(&addr, P::seqpacket, RICSA(&sun), sizeof(sa_family_t) + 8);
  EXPECT_OK(r);
  if (r) {
    EXPECT_EQ("unixpacket", addr.protocol());
    EXPECT_EQ("@foo/bar", addr.address());
  }
}

TEST(UnixProtocol, Parse) {
  auto p = net::unixprotocol();

  net::Addr out;

  base::Result r = p->parse(&out, "unix", "");
  EXPECT_OK(r);
  if (r) {
    EXPECT_EQ("unix", out.protocol());
    EXPECT_EQ("", out.address());
    EXPECT_EQ("", out.ip());
    EXPECT_EQ(0, out.port());
    auto raw = out.raw();
    EXPECT_EQ(raw.second, sizeof(sa_family_t));
    EXPECT_EQ(AF_UNIX, RICSUN(raw.first)->sun_family);
  }

  r = p->parse(&out, "unixgram", "/foo/bar");
  EXPECT_OK(r);
  if (r) {
    EXPECT_EQ("unixgram", out.protocol());
    EXPECT_EQ("/foo/bar", out.address());
    EXPECT_EQ("", out.ip());
    EXPECT_EQ(0, out.port());
    auto raw = out.raw();
    EXPECT_EQ(raw.second, sizeof(sa_family_t) + 9);
    EXPECT_EQ(AF_UNIX, RICSUN(raw.first)->sun_family);
    std::string path(RICSUN(raw.first)->sun_path,
                     raw.second - sizeof(sa_family_t));
    std::string expected("/foo/bar\000", 9);
    EXPECT_EQ(expected, path);
  }

  r = p->parse(&out, "unixpacket", "@foo/bar");
  EXPECT_OK(r);
  if (r) {
    EXPECT_EQ("unixpacket", out.protocol());
    EXPECT_EQ("@foo/bar", out.address());
    EXPECT_EQ("", out.ip());
    EXPECT_EQ(0, out.port());
    auto raw = out.raw();
    EXPECT_EQ(raw.second, sizeof(sa_family_t) + 8);
    EXPECT_EQ(AF_UNIX, RICSUN(raw.first)->sun_family);
    std::string path(RICSUN(raw.first)->sun_path,
                     raw.second - sizeof(sa_family_t));
    std::string expected("\000foo/bar", 8);
    EXPECT_EQ(expected, path);
  }
}

TEST(UnixProtocol, ListenAndDial) {
  std::string path = "@mojo/net/unix_test/listen";
  const char* seed = ::getenv("TEST_RANDOM_SEED");
  if (seed) {
    path += '/';
    path += seed;
  }
  auto p = net::unixprotocol();
  net::Addr addr = net::unixaddr(P::stream, path);
  TestListenAndDial(p, addr);
}

static void init() __attribute__((constructor));
static void init() { base::log_stderr_set_level(VLOG_LEVEL(6)); }
