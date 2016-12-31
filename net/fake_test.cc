// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <thread>
#include <vector>

#include "base/logging.h"
#include "base/result_testing.h"
#include "net/addr.h"
#include "net/conn.h"
#include "net/fake.h"
#include "net/protocol.h"
#include "net/registry.h"

using P = net::ProtocolType;
using RC = base::Result::Code;

static const char* RICCH(const uint32_t* ptr) {
  return reinterpret_cast<const char*>(ptr);
}

static char* RICH(uint32_t* ptr) { return reinterpret_cast<char*>(ptr); }

TEST(FakeAddr, Basics) {
  net::FakeData data;

  auto addr = net::fakeaddr(&data, P::stream, 0xf005ba11);
  EXPECT_EQ("fake", addr.protocol());
  EXPECT_EQ(P::stream, addr.protocol_type());
  EXPECT_EQ("0xf005ba11", addr.address());
  EXPECT_EQ("\xF0\x05\xBA\x11", addr.raw_string());

  auto addr2 = net::fakeaddr(&data, P::stream, 0xf005ba11);
  EXPECT_TRUE(addr == addr2);  // memoized

  auto addr3 = net::fakeaddr(P::stream, 0xf005ba11);
  EXPECT_TRUE(addr == addr3);  // not memoized
}

TEST(FakeConn, EndToEnd) {
  net::Options o;
  const auto& m = o.io().manager();

  net::FakeData data;
  auto p = net::fakeprotocol(&data);

  auto la = net::fakeaddr(&data, P::stream, 0x6c6f636c);
  auto ra = net::fakeaddr(&data, P::stream, 0x72656d74);

  std::vector<std::thread> vec;

  auto acceptfn = [&vec](net::Conn c) {
    auto closure = [c] {
      io::Reader r = c.reader();
      io::Writer w = c.writer();
      while (true) {
        base::Result result;

        uint32_t ka, kb, kc;
        ka = kb = kc = ~uint32_t(0);
        std::size_t n;
        result = r.read_exactly(RICH(&ka), &n, sizeof(ka));
        if (result.code() == RC::END_OF_FILE) break;
        CHECK_OK(result);
        CHECK_EQ(n, sizeof(ka));
        CHECK_OK(r.read_exactly(RICH(&kb), &n, sizeof(kb)));
        CHECK_EQ(n, sizeof(kb));
        CHECK_OK(r.read_exactly(RICH(&kc), &n, sizeof(kc)));
        CHECK_EQ(n, sizeof(kc));

        uint32_t kx = 5;
        uint32_t ky = ka * kx * kx + kb * kx + kc;
        CHECK_OK(w.write(&n, RICCH(&ky), sizeof(ky)));
        CHECK_EQ(n, sizeof(ky));
      }
      EXPECT_OK(w.close());
    };
    vec.emplace_back(std::move(closure));
  };

  event::Task task0;
  net::ListenConn l;
  p->listen(&task0, &l, ra, o, acceptfn);
  event::wait(m, &task0);
  EXPECT_OK(task0.result());

  event::Task task1;
  l.start(&task1);
  event::wait(m, &task1);
  EXPECT_OK(task1.result());

  event::Task task2;
  net::Conn c;
  p->dial(&task2, &c, ra, la, o);
  event::wait(m, &task2);
  EXPECT_OK(task2.result());

  EXPECT_EQ("fake", c.local_addr().protocol());
  EXPECT_EQ("locl", c.local_addr().raw_string());
  EXPECT_EQ("fake", c.remote_addr().protocol());
  EXPECT_EQ("remt", c.remote_addr().raw_string());

  io::Reader r = c.reader();
  io::Writer w = c.writer();

  uint32_t ka = 1, kb = 2, kc = 4;
  uint32_t ky = ~uint32_t(0);
  std::size_t n;

  EXPECT_OK(w.write(&n, RICCH(&ka), sizeof(ka)));
  EXPECT_EQ(sizeof(ka), n);
  EXPECT_OK(w.write(&n, RICCH(&kb), sizeof(kb)));
  EXPECT_EQ(sizeof(kb), n);
  EXPECT_OK(w.write(&n, RICCH(&kc), sizeof(kc)));
  EXPECT_EQ(sizeof(kc), n);

  EXPECT_OK(r.read_exactly(RICH(&ky), &n, sizeof(ky)));
  EXPECT_EQ(sizeof(ky), n);
  EXPECT_EQ(39U, ky);

  EXPECT_OK(w.close());
  EXPECT_EOF(r.read_exactly(RICH(&ky), &n, sizeof(ky)));
  EXPECT_EQ(0U, n);

  EXPECT_OK(c.close());
  EXPECT_OK(l.close());

  for (auto& t : vec) t.join();
}

TEST(FakeProtocol, ParseResolve) {
  net::Options o;
  const auto& m = o.io().manager();

  net::Registry registry;
  base::token_t token;
  net::FakeData data;
  registry.add(&token, 100, net::fakeprotocol(&data));

  data.names["foo"].push_back(0x01020304);
  data.names["bar"].push_back(0x666f6f73);
  data.names["bar"].push_back(0x62616c6c);

  EXPECT_TRUE(registry.supports("fake"));
  EXPECT_FALSE(registry.supports("ekaf"));

  net::Addr addr;
  EXPECT_OK(registry.parse(&addr, "fake", "0xf005ba11"));
  EXPECT_EQ("fake", addr.protocol());
  EXPECT_EQ(P::stream, addr.protocol_type());
  EXPECT_EQ("0xf005ba11", addr.address());
  EXPECT_EQ("\xF0\x05\xBA\x11", addr.raw_string());

  EXPECT_NOT_IMPLEMENTED(registry.parse(&addr, "ekaf", "0xf005ba11"));

  std::vector<net::Addr> addrs;
  EXPECT_OK(registry.resolve(&addrs, "fake", "foo"));
  EXPECT_EQ(1U, addrs.size());
  if (addrs.size() > 0) {
    EXPECT_EQ("fake", addrs[0].protocol());
    EXPECT_EQ("0x01020304", addrs[0].address());
    EXPECT_EQ("\x01\x02\x03\x04", addrs[0].raw_string());
  }

  EXPECT_OK(registry.resolve(&addrs, "fake", "bar"));
  EXPECT_EQ(3U, addrs.size());
  if (addrs.size() > 1) {
    EXPECT_EQ("fake", addrs[1].protocol());
    EXPECT_EQ("0x666f6f73", addrs[1].address());
    EXPECT_EQ("foos", addrs[1].raw_string());
  }
  if (addrs.size() > 2) {
    EXPECT_EQ("fake", addrs[2].protocol());
    EXPECT_EQ("0x62616c6c", addrs[2].address());
    EXPECT_EQ("ball", addrs[2].raw_string());
  }

  EXPECT_NOT_FOUND(registry.resolve(&addrs, "fake", "baz"));

  EXPECT_NOT_IMPLEMENTED(registry.resolve(&addrs, "ekaf", "foo"));
}

static void init() __attribute__((constructor));
static void init() { base::log_stderr_set_level(VLOG_LEVEL(6)); }
