// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/result_testing.h"
#include "event/manager.h"
#include "event/task.h"
#include "net/addr.h"
#include "net/fake.h"
#include "net/registry.h"

TEST(Registry, Thunk) {
  event::ManagerOptions mo;
  mo.set_async_mode();

  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options o;
  o.get<io::Options>().manager = m;

  net::FakeData data;
  data.names["foo"] = {0x01020304};
  data.names["bar"] = {0x68656c70};
  net::Registry r;
  r.add(nullptr, 0, net::fakeprotocol(&data));

  EXPECT_TRUE(r.supports("fake"));
  EXPECT_FALSE(r.supports("ekaf"));

  net::Addr addr;
  EXPECT_OK(r.parse(&addr, "fake", "0xf005ba11"));
  EXPECT_EQ("fake", addr.protocol());
  EXPECT_EQ("0xf005ba11", addr.address());
  EXPECT_EQ("\xF0\x05\xBA\x11", addr.raw_string());
  EXPECT_NOT_IMPLEMENTED(r.parse(&addr, "ekaf", "0xf005ba11"));

  std::vector<net::Addr> addrs;
  EXPECT_OK(r.resolve(&addrs, "fake", "foo", o));
  EXPECT_EQ(1U, addrs.size());
  if (addrs.size() > 0) {
    EXPECT_EQ("fake", addrs[0].protocol());
    EXPECT_EQ("0x01020304", addrs[0].address());
    EXPECT_EQ("\x01\x02\x03\x04", addrs[0].raw_string());
  }

  EXPECT_OK(r.resolve(&addrs, "fake", "bar", o));
  EXPECT_EQ(2U, addrs.size());
  if (addrs.size() > 1) {
    EXPECT_EQ("fake", addrs[1].protocol());
    EXPECT_EQ("0x68656c70", addrs[1].address());
    EXPECT_EQ("help", addrs[1].raw_string());
  }
}
