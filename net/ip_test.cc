// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "base/concat.h"
#include "base/result_testing.h"
#include "net/ip.h"

namespace {
enum class V : uint8_t {
  v4 = 1,
  v6 = 2,
  neither = 3,
};

enum class U : bool {
  no = false,
  yes = true,
};

enum class L : bool {
  no = false,
  yes = true,
};

enum class S : uint8_t {
  node = 1,
  link = 2,
  site = 3,
  global = 4,
  none = 5,
};

enum class T : uint8_t {
  uni = 1,
  multi = 3,
  broad = 4,
  none = 5,
};

struct TestEntry {
  net::IP ip;
  char v;
  char u;
  char l;
  char s;
  char t;
};
}  // anonymous namespace

static constexpr auto v4 = net::IP::v4;
static constexpr auto v6 = net::IP::v6;

static constexpr uint16_t f = 0xffff;

static const TestEntry kTestTable[] = {
    {{}, '-', '-', '-', '-', '-'},

    {{v4, 0x00, 0x00, 0x00, 0x00}, '4', 'u', '-', '-', '-'},  // 0.0.0.0
    {{v4, 0x00, 0x00, 0x00, 0x01}, '4', '-', '-', '-', '-'},
    {{v4, 0x00, 0x01, 0x02, 0x03}, '4', '-', '-', '-', '-'},
    {{v4, 0x00, 0xff, 0xff, 0xff}, '4', '-', '-', '-', '-'},

    {{v4, 0x01, 0x00, 0x00, 0x00}, '4', '-', '-', 'G', 'U'},
    {{v4, 0x08, 0x08, 0x04, 0x04}, '4', '-', '-', 'G', 'U'},  // 8.8.4.4
    {{v4, 0x09, 0xff, 0xff, 0xff}, '4', '-', '-', 'G', 'U'},

    {{v4, 0x0a, 0x00, 0x00, 0x00}, '4', '-', '-', 'S', 'U'},
    {{v4, 0x0a, 0x01, 0x02, 0x03}, '4', '-', '-', 'S', 'U'},  // 10.1.2.3
    {{v4, 0x0a, 0xff, 0xff, 0xff}, '4', '-', '-', 'S', 'U'},

    {{v4, 0x0b, 0x00, 0x00, 0x00}, '4', '-', '-', 'G', 'U'},
    {{v4, 0x23, 0x45, 0x67, 0x89}, '4', '-', '-', 'G', 'U'},
    {{v4, 0x7e, 0xff, 0xff, 0xff}, '4', '-', '-', 'G', 'U'},

    {{v4, 0x7f, 0x00, 0x00, 0x00}, '4', '-', 'l', 'N', 'U'},
    {{v4, 0x7f, 0x00, 0x00, 0x01}, '4', '-', 'l', 'N', 'U'},  // 127.0.0.1
    {{v4, 0x7f, 0xff, 0xff, 0xff}, '4', '-', 'l', 'N', 'U'},

    {{v4, 0x80, 0x00, 0x00, 0x00}, '4', '-', '-', 'G', 'U'},
    {{v4, 0xa9, 0xfd, 0xff, 0xff}, '4', '-', '-', 'G', 'U'},

    {{v4, 0xa9, 0xfe, 0x00, 0x00}, '4', '-', '-', 'L', 'U'},
    {{v4, 0xa9, 0xfe, 0x01, 0x02}, '4', '-', '-', 'L', 'U'},  // 169.254.1.2
    {{v4, 0xa9, 0xfe, 0xff, 0xff}, '4', '-', '-', 'L', 'U'},

    {{v4, 0xa9, 0xff, 0x00, 0x00}, '4', '-', '-', 'G', 'U'},
    {{v4, 0xac, 0x0f, 0xff, 0xff}, '4', '-', '-', 'G', 'U'},

    {{v4, 0xac, 0x10, 0x00, 0x00}, '4', '-', '-', 'S', 'U'},
    {{v4, 0xac, 0x10, 0x02, 0x01}, '4', '-', '-', 'S', 'U'},  // 172.16.2.1
    {{v4, 0xac, 0x1f, 0xff, 0xff}, '4', '-', '-', 'S', 'U'},

    {{v4, 0xac, 0x20, 0x00, 0x00}, '4', '-', '-', 'G', 'U'},
    {{v4, 0xc0, 0xa7, 0xff, 0xff}, '4', '-', '-', 'G', 'U'},

    {{v4, 0xc0, 0xa8, 0x00, 0x00}, '4', '-', '-', 'S', 'U'},
    {{v4, 0xc0, 0xa8, 0x02, 0x01}, '4', '-', '-', 'S', 'U'},  // 192.168.2.1
    {{v4, 0xc0, 0xa8, 0xff, 0xff}, '4', '-', '-', 'S', 'U'},

    {{v4, 0xc0, 0xa9, 0x00, 0x00}, '4', '-', '-', 'G', 'U'},
    {{v4, 0xc9, 0x02, 0x03, 0x04}, '4', '-', '-', 'G', 'U'},  // 201.2.3.4
    {{v4, 0xdf, 0xff, 0xff, 0xff}, '4', '-', '-', 'G', 'U'},

    {{v4, 0xe0, 0x00, 0x00, 0x00}, '4', '-', '-', 'L', 'M'},
    {{v4, 0xe0, 0x00, 0x00, 0x01}, '4', '-', '-', 'L', 'M'},  // 224.0.0.1
    {{v4, 0xe0, 0x00, 0x00, 0xff}, '4', '-', '-', 'L', 'M'},

    {{v4, 0xe0, 0x00, 0x01, 0x00}, '4', '-', '-', 'G', 'M'},
    {{v4, 0xe7, 0x01, 0x02, 0x03}, '4', '-', '-', 'G', 'M'},
    {{v4, 0xee, 0xff, 0xff, 0xff}, '4', '-', '-', 'G', 'M'},

    {{v4, 0xef, 0x00, 0x00, 0x00}, '4', '-', '-', 'S', 'M'},
    {{v4, 0xef, 0x01, 0x02, 0x03}, '4', '-', '-', 'S', 'M'},  // 239.1.2.3
    {{v4, 0xef, 0xff, 0xff, 0xff}, '4', '-', '-', 'S', 'M'},

    {{v4, 0xf0, 0x00, 0x00, 0x00}, '4', '-', '-', '-', '-'},
    {{v4, 0xf7, 0x01, 0x02, 0x03}, '4', '-', '-', '-', '-'},
    {{v4, 0xff, 0xff, 0xff, 0xfe}, '4', '-', '-', '-', '-'},

    {{v4, 0xff, 0xff, 0xff, 0xff}, '4', '-', '-', 'L', 'B'},  // 255.255.255.255

    {{v6, 0x0000, 0, 0, 0, 0, 0, 0, 0}, '6', 'u', '-', '-', '-'},  // ::
    {{v6, 0x0000, 0, 0, 0, 0, 0, 0, 1}, '6', '-', 'l', 'N', 'U'},  // ::1
    {{v6, 0x0000, 0, 0, 0, 0, 0, 0, 2}, '6', '-', '-', '-', '-'},  // ::2
    {{v6, 0x1000, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', '-', '-'},  // 1000::
    {{v6, 0x1fff, f, f, f, f, f, f, f}, '6', '-', '-', '-', '-'},  // 1fff:...
    {{v6, 0x2000, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', 'G', 'U'},  // 2000::
    {{v6, 0x3fff, f, f, f, f, f, f, f}, '6', '-', '-', 'G', 'U'},  // 3fff:...
    {{v6, 0xfe80, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', 'L', 'U'},  // fe80::
    {{v6, 0xfc00, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', 'S', 'U'},  // fc00::
    {{v6, 0xff00, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', '-', 'M'},  // ff00::
    {{v6, 0xff01, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', 'N', 'M'},  // ff01::
    {{v6, 0xff02, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', 'L', 'M'},  // ff02::
    {{v6, 0xff03, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', '-', 'M'},  // ff03::
    {{v6, 0xff04, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', '-', 'M'},  // ff04::
    {{v6, 0xff05, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', 'S', 'M'},  // ff05::
    {{v6, 0xff06, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', '-', 'M'},  // ff06::
    {{v6, 0xff0d, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', '-', 'M'},  // ff0d::
    {{v6, 0xff0e, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', 'G', 'M'},  // ff0e::
    {{v6, 0xff0f, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', '-', 'M'},  // ff0f::
    {{v6, 0xffc1, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', 'N', 'M'},  // ffc1::
    {{v6, 0xffd2, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', 'L', 'M'},  // ffd2::
    {{v6, 0xffe5, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', 'S', 'M'},  // ffe5::
    {{v6, 0xfffe, 0, 0, 0, 0, 0, 0, 0}, '6', '-', '-', 'G', 'M'},  // fffe::
};
static const std::size_t kTestTableLen = sizeof(kTestTable) / sizeof(TestEntry);

void TestClassify(net::IP ip, char v, char u, char l, char s, char t) {
  switch (v) {
    case '4':
      EXPECT_TRUE(ip.is_ipv4());
      EXPECT_FALSE(ip.is_ipv6());
      break;
    case '6':
      EXPECT_FALSE(ip.is_ipv4());
      EXPECT_TRUE(ip.is_ipv6());
      break;
    case '-':
      EXPECT_FALSE(ip.is_ipv4());
      EXPECT_FALSE(ip.is_ipv6());
      break;
    default:
      LOG(FATAL) << "BUG!";
  }
  switch (u) {
    case 'u':
      EXPECT_TRUE(ip.is_unspecified());
      break;
    case '-':
      EXPECT_FALSE(ip.is_unspecified());
      break;
    default:
      LOG(FATAL) << "BUG!";
  }
  switch (l) {
    case 'l':
      EXPECT_TRUE(ip.is_loopback());
      break;
    case '-':
      EXPECT_FALSE(ip.is_loopback());
      break;
    default:
      LOG(FATAL) << "BUG!";
  }
  switch (s) {
    case 'N':
      EXPECT_TRUE(ip.is_node_local());
      EXPECT_FALSE(ip.is_link_local());
      EXPECT_FALSE(ip.is_site_local());
      EXPECT_FALSE(ip.is_global());
      break;
    case 'L':
      EXPECT_FALSE(ip.is_node_local());
      EXPECT_TRUE(ip.is_link_local());
      EXPECT_FALSE(ip.is_site_local());
      EXPECT_FALSE(ip.is_global());
      break;
    case 'S':
      EXPECT_FALSE(ip.is_node_local());
      EXPECT_FALSE(ip.is_link_local());
      EXPECT_TRUE(ip.is_site_local());
      EXPECT_FALSE(ip.is_global());
      break;
    case 'G':
      EXPECT_FALSE(ip.is_node_local());
      EXPECT_FALSE(ip.is_link_local());
      EXPECT_FALSE(ip.is_site_local());
      EXPECT_TRUE(ip.is_global());
      break;
    case '-':
      EXPECT_FALSE(ip.is_node_local());
      EXPECT_FALSE(ip.is_link_local());
      EXPECT_FALSE(ip.is_site_local());
      EXPECT_FALSE(ip.is_global());
      break;
    default:
      LOG(FATAL) << "BUG!";
  }
  switch (t) {
    case 'U':
      EXPECT_TRUE(ip.is_unicast());
      EXPECT_FALSE(ip.is_multicast());
      EXPECT_FALSE(ip.is_broadcast());
      break;
    case 'M':
      EXPECT_FALSE(ip.is_unicast());
      EXPECT_TRUE(ip.is_multicast());
      EXPECT_FALSE(ip.is_broadcast());
      break;
    case 'B':
      EXPECT_FALSE(ip.is_unicast());
      EXPECT_FALSE(ip.is_multicast());
      EXPECT_TRUE(ip.is_broadcast());
      break;
    case '-':
      EXPECT_FALSE(ip.is_unicast());
      EXPECT_FALSE(ip.is_multicast());
      EXPECT_FALSE(ip.is_broadcast());
      break;
    default:
      LOG(FATAL) << "BUG!";
  }
}

TEST(IP, Classify) {
  for (std::size_t i = 0; i < kTestTableLen; ++i) {
    SCOPED_TRACE(i);
    const auto& e = kTestTable[i];
    {
      SCOPED_TRACE("normal");
      TestClassify(e.ip, e.v, e.u, e.l, e.s, e.t);
    }
    {
      SCOPED_TRACE("widened");
      TestClassify(e.ip.as_wide(), e.v, e.u, e.l, e.s, e.t);
    }
  }
}

TEST(IP, ParseAndStringify) {
  net::IP ip;

  EXPECT_FALSE(ip);
  EXPECT_EQ("", ip.as_string());

  EXPECT_OK(net::IP::parse(&ip, "0.0.0.0"));
  EXPECT_EQ("0.0.0.0", ip.as_string());

  EXPECT_OK(net::IP::parse(&ip, "127.0.0.1"));
  EXPECT_EQ("127.0.0.1", ip.as_string());
  ip.widen();
  EXPECT_EQ("::ffff:127.0.0.1", ip.as_string());

  EXPECT_OK(net::IP::parse(&ip, "255.255.255.255"));
  EXPECT_EQ("255.255.255.255", ip.as_string());

  EXPECT_OK(net::IP::parse(&ip, "::"));
  EXPECT_EQ("::", ip.as_string());

  EXPECT_OK(net::IP::parse(&ip, "::1"));
  EXPECT_EQ("::1", ip.as_string());

  EXPECT_OK(net::IP::parse(&ip, "::ffff:1.2.3.4"));
  EXPECT_EQ("::ffff:1.2.3.4", ip.as_string());
  ip.narrow();
  EXPECT_EQ("1.2.3.4", ip.as_string());

  EXPECT_OK(net::IP::parse(&ip, "2001:0db8::1"));
  EXPECT_EQ("2001:db8::1", ip.as_string());

  EXPECT_INVALID_ARGUMENT(net::IP::parse(&ip, "localhost"));
  EXPECT_FALSE(ip);
}

TEST(IP, UnspecifiedV4) {
  net::IP ip = net::IP::unspecified_v4();
  EXPECT_TRUE(ip);
  EXPECT_TRUE(ip.is_ipv4());
  EXPECT_FALSE(ip.is_ipv6());
  EXPECT_EQ("0.0.0.0", ip.as_string());
  EXPECT_EQ(4U, ip.size());
  std::string expected("\0\0\0\0", 4);
  EXPECT_EQ(expected, ip.raw_string());
}

TEST(IP, UnspecifiedV6) {
  net::IP ip = net::IP::unspecified_v6();
  EXPECT_TRUE(ip);
  EXPECT_TRUE(ip.is_ipv6());
  EXPECT_FALSE(ip.is_ipv4());
  EXPECT_EQ("::", ip.as_string());
  EXPECT_EQ(16U, ip.size());
  std::string expected("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16);
  EXPECT_EQ(expected, ip.raw_string());
}

TEST(IP, Compare) {
  std::vector<net::IP> testdata{
      {},
      {v6, 0, 0, 0, 0, 0, 0, 0, 0},
      {v6, 0, 0, 0, 0, 0, 0, 0, 1},
      {v4, 0, 0, 0, 0},
      {v4, 8, 8, 4, 4},
      {v4, 127, 0, 0, 1},
      {v4, 255, 255, 255, 255},
      {v6, 0x2001, 0xdb8, 0, 0, 0, 0, 0, 1},
      {v6, 0xfe80, 0, 0, 0, 0xeea8, 0x6bff, 0xfeff, 0x8c92},
      {v6, 0xff01, 0, 0, 0, 0, 0, 0, 1},
      {v6, 0xff02, 0, 0, 0, 0, 0, 0, 1},
      {v6, 0xff05, 0, 0, 0, 0, 0, 0, 1},
  };
  std::hash<net::IP> hash;

  for (const auto& ip : testdata) {
    SCOPED_TRACE(ip);

    EXPECT_TRUE(ip == ip);
    EXPECT_FALSE(ip != ip);
    EXPECT_EQ(hash(ip), hash(ip));

    EXPECT_FALSE(ip < ip);
    EXPECT_TRUE(ip <= ip);
    EXPECT_FALSE(ip > ip);
    EXPECT_TRUE(ip >= ip);

    auto ipw = ip.as_wide();

    EXPECT_TRUE(ipw == ipw);
    EXPECT_FALSE(ipw != ipw);
    EXPECT_EQ(hash(ipw), hash(ipw));

    EXPECT_FALSE(ipw < ipw);
    EXPECT_TRUE(ipw <= ipw);
    EXPECT_FALSE(ipw > ipw);
    EXPECT_TRUE(ipw >= ipw);

    EXPECT_TRUE(ip == ipw);
    EXPECT_FALSE(ip != ipw);
    EXPECT_EQ(hash(ip), hash(ipw));

    EXPECT_FALSE(ip < ipw);
    EXPECT_TRUE(ip <= ipw);
    EXPECT_FALSE(ip > ipw);
    EXPECT_TRUE(ip >= ipw);

    EXPECT_FALSE(ipw < ip);
    EXPECT_TRUE(ipw <= ip);
    EXPECT_FALSE(ipw > ip);
    EXPECT_TRUE(ipw >= ip);
  }
  for (std::size_t i = 1, n = testdata.size(); i < n; ++i) {
    const auto& a = testdata[i - 1];
    const auto& b = testdata[i];
    SCOPED_TRACE(base::concat(a, " vs ", b));

    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(a <= b);
    EXPECT_FALSE(a > b);
    EXPECT_FALSE(a >= b);

    EXPECT_FALSE(b == a);
    EXPECT_TRUE(b != a);
    EXPECT_FALSE(b < a);
    EXPECT_FALSE(b <= a);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(b >= a);

    auto aw = a.as_wide();
    auto bw = b.as_wide();

    EXPECT_FALSE(aw == bw);
    EXPECT_TRUE(aw != bw);
    EXPECT_TRUE(aw < bw);
    EXPECT_TRUE(aw <= bw);
    EXPECT_FALSE(aw > bw);
    EXPECT_FALSE(aw >= bw);

    EXPECT_FALSE(bw == aw);
    EXPECT_TRUE(bw != aw);
    EXPECT_FALSE(bw < aw);
    EXPECT_FALSE(bw <= aw);
    EXPECT_TRUE(bw > aw);
    EXPECT_TRUE(bw >= aw);
  }
}

TEST(CIDR, Basics) {
  net::CIDR cidr;

  {
    SCOPED_TRACE("empty");
    EXPECT_FALSE(cidr);
    EXPECT_EQ("", cidr.as_string());
    EXPECT_FALSE(cidr.contains(net::IP()));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 8, 8, 8, 8)));
    EXPECT_FALSE(cidr.contains(net::IP(v6, 0x2001, 0xdb8, 0, 0, 0, 0, 0, 1)));
  }

  {
    SCOPED_TRACE("0.0.0.0/0");
    EXPECT_OK(net::CIDR::parse(&cidr, "0.0.0.0/0"));
    EXPECT_EQ("0.0.0.0/0", cidr.as_string());
    EXPECT_EQ("0.0.0.0", cidr.first().as_string());
    EXPECT_EQ("255.255.255.255", cidr.last().as_string());
    EXPECT_FALSE(cidr.contains(net::IP()));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 0, 0, 0, 0)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 8, 8, 8, 8)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 127, 0, 0, 1)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 255, 255, 255, 255)));
    EXPECT_FALSE(cidr.contains(net::IP(v6, 0x2001, 0xdb8, 0, 0, 0, 0, 0, 1)));
  }

  {
    SCOPED_TRACE("127.0.0.1/8");
    EXPECT_OK(net::CIDR::parse(&cidr, "127.0.0.1/8"));
    EXPECT_EQ("127.0.0.0/8", cidr.as_string());
    EXPECT_EQ("127.0.0.0", cidr.first().as_string());
    EXPECT_EQ("127.255.255.255", cidr.last().as_string());
    EXPECT_FALSE(cidr.contains(net::IP(v4, 8, 8, 8, 8)));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 126, 255, 255, 255)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 127, 0, 0, 0)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 127, 0, 0, 1)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 127, 255, 255, 255)));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 128, 0, 0, 0)));
    EXPECT_FALSE(cidr.contains(net::IP(v6, 0x2001, 0xdb8, 0, 0, 0, 0, 0, 1)));
  }

  {
    SCOPED_TRACE("255.255.255.255/32");
    EXPECT_OK(net::CIDR::parse(&cidr, "255.255.255.255/32"));
    EXPECT_EQ("255.255.255.255/32", cidr.as_string());
    EXPECT_EQ("255.255.255.255", cidr.first().as_string());
    EXPECT_EQ("255.255.255.255", cidr.last().as_string());
    EXPECT_FALSE(cidr.contains(net::IP(v4, 8, 8, 8, 8)));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 127, 0, 0, 1)));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 255, 255, 255, 254)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 255, 255, 255, 255)));
    EXPECT_FALSE(cidr.contains(net::IP(v6, 0x2001, 0xdb8, 0, 0, 0, 0, 0, 1)));
  }

  {
    SCOPED_TRACE("::/0");
    EXPECT_OK(net::CIDR::parse(&cidr, "::/0"));
    EXPECT_EQ("::/0", cidr.as_string());
    EXPECT_FALSE(cidr.contains(net::IP()));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 0, 0, 0, 0)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 8, 8, 8, 8)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 127, 0, 0, 1)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 255, 255, 255, 255)));
    EXPECT_TRUE(cidr.contains(net::IP(v6, 0x2001, 0xdb8, 0, 0, 0, 0, 0, 1)));
    EXPECT_TRUE(cidr.contains(
        net::IP(v6, 0xfe80, 0, 0, 0, 0xeea8, 0x6bff, 0xfeff, 0x8c92)));
  }

  {
    SCOPED_TRACE("::ffff:0.0.0.0/96");
    EXPECT_OK(net::CIDR::parse(&cidr, "::ffff:0.0.0.0/96"));
    EXPECT_EQ("::ffff:0.0.0.0/96", cidr.as_string());
    EXPECT_FALSE(cidr.contains(net::IP()));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 0, 0, 0, 0)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 8, 8, 8, 8)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 127, 0, 0, 1)));
    EXPECT_TRUE(cidr.contains(net::IP(v4, 255, 255, 255, 255)));
    EXPECT_FALSE(cidr.contains(net::IP(v6, 0x2001, 0xdb8, 0, 0, 0, 0, 0, 1)));
    EXPECT_FALSE(cidr.contains(
        net::IP(v6, 0xfe80, 0, 0, 0, 0xeea8, 0x6bff, 0xfeff, 0x8c92)));
  }

  {
    SCOPED_TRACE("2000::/3");
    EXPECT_OK(net::CIDR::parse(&cidr, "2000::/3"));
    EXPECT_EQ("2000::/3", cidr.as_string());
    EXPECT_FALSE(cidr.contains(net::IP()));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 0, 0, 0, 0)));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 8, 8, 8, 8)));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 127, 0, 0, 1)));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 255, 255, 255, 255)));
    EXPECT_TRUE(cidr.contains(net::IP(v6, 0x2001, 0xdb8, 0, 0, 0, 0, 0, 1)));
    EXPECT_FALSE(cidr.contains(
        net::IP(v6, 0xfe80, 0, 0, 0, 0xeea8, 0x6bff, 0xfeff, 0x8c92)));
  }

  {
    SCOPED_TRACE("fe80::/10");
    EXPECT_OK(net::CIDR::parse(&cidr, "fe80::/10"));
    EXPECT_EQ("fe80::/10", cidr.as_string());
    EXPECT_FALSE(cidr.contains(net::IP()));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 0, 0, 0, 0)));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 8, 8, 8, 8)));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 127, 0, 0, 1)));
    EXPECT_FALSE(cidr.contains(net::IP(v4, 255, 255, 255, 255)));
    EXPECT_FALSE(cidr.contains(net::IP(v6, 0x2001, 0xdb8, 0, 0, 0, 0, 0, 1)));
    EXPECT_TRUE(cidr.contains(
        net::IP(v6, 0xfe80, 0, 0, 0, 0xeea8, 0x6bff, 0xfeff, 0x8c92)));
  }
}

TEST(CIDR, Compare) {
  std::vector<net::CIDR> testdata{
      {},
      {{v6, 0, 0, 0, 0, 0, 0, 0, 0}, 0},
      {{v6, 0x2000, 0, 0, 0, 0, 0, 0, 0}, 3},
      {{v6, 0, 0, 0, 0, 0, 0, 0, 0}, 8},
      {{v6, 0xff00, 0, 0, 0, 0, 0, 0, 0}, 8},
      {{v6, 0xfe80, 0, 0, 0, 0, 0, 0, 0}, 10},
      {{v6, 0xff05, 0, 0, 0, 0, 0, 0, 0}, 16},
      {{v6, 0x2001, 0xdb8, 0, 0, 0, 0, 0, 0}, 32},
      {{v6, 0, 0, 0, 0, 0, f - 1, 0, 0}, 96},
      {{v4, 0, 0, 0, 0}, 0},
      {{v6, 0, 0, 0, 0, 1, 0, 0, 0}, 96},
      {{v4, 0, 0, 0, 0}, 8},
      {{v4, 127, 0, 0, 0}, 8},
      {{v6, 0, 0, 0, 0, 0, 0, 0, 0}, 128},
      {{v6, 0, 0, 0, 0, 0, 0, 0, 1}, 128},
      {{v4, 0, 0, 0, 0}, 32},
      {{v4, 8, 8, 4, 4}, 32},
      {{v4, 127, 0, 0, 1}, 32},
      {{v4, 255, 255, 255, 255}, 32},
      {{v6, 0x2001, 0xdb8, 0, 0, 0, 0, 0, 1}, 128},
      {{v6, 0xfe80, 0, 0, 0, 0xeea8, 0x6bff, 0xfeff, 0x8c92}, 128},
      {{v6, 0xff01, 0, 0, 0, 0, 0, 0, 1}, 128},
      {{v6, 0xff02, 0, 0, 0, 0, 0, 0, 1}, 128},
      {{v6, 0xff05, 0, 0, 0, 0, 0, 0, 1}, 128},
  };
  std::hash<net::CIDR> hash;

  for (const auto& cidr : testdata) {
    SCOPED_TRACE(cidr);

    EXPECT_TRUE(cidr == cidr);
    EXPECT_FALSE(cidr != cidr);
    EXPECT_EQ(hash(cidr), hash(cidr));

    EXPECT_FALSE(cidr < cidr);
    EXPECT_TRUE(cidr <= cidr);
    EXPECT_FALSE(cidr > cidr);
    EXPECT_TRUE(cidr >= cidr);

    auto cidrw = cidr.as_wide();
    EXPECT_TRUE(cidrw == cidrw);
    EXPECT_FALSE(cidrw != cidrw);
    EXPECT_EQ(hash(cidrw), hash(cidrw));

    EXPECT_FALSE(cidrw < cidrw);
    EXPECT_TRUE(cidrw <= cidrw);
    EXPECT_FALSE(cidrw > cidrw);
    EXPECT_TRUE(cidrw >= cidrw);

    EXPECT_TRUE(cidr == cidrw);
    EXPECT_FALSE(cidr != cidrw);
    EXPECT_EQ(hash(cidr), hash(cidrw));

    EXPECT_FALSE(cidr < cidrw);
    EXPECT_TRUE(cidr <= cidrw);
    EXPECT_FALSE(cidr > cidrw);
    EXPECT_TRUE(cidr >= cidrw);

    EXPECT_TRUE(cidrw == cidr);
    EXPECT_FALSE(cidrw != cidr);
    EXPECT_EQ(hash(cidrw), hash(cidr));

    EXPECT_FALSE(cidrw < cidr);
    EXPECT_TRUE(cidrw <= cidr);
    EXPECT_FALSE(cidrw > cidr);
    EXPECT_TRUE(cidrw >= cidr);
  }
  for (std::size_t i = 1, n = testdata.size(); i < n; ++i) {
    const auto& a = testdata[i - 1];
    const auto& b = testdata[i];
    SCOPED_TRACE(base::concat(a, " vs ", b));

    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(a <= b);
    EXPECT_FALSE(a > b);
    EXPECT_FALSE(a >= b);

    EXPECT_FALSE(b == a);
    EXPECT_TRUE(b != a);
    EXPECT_FALSE(b < a);
    EXPECT_FALSE(b <= a);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(b >= a);

    auto aw = a.as_wide();
    auto bw = b.as_wide();

    EXPECT_FALSE(aw == bw);
    EXPECT_TRUE(aw != bw);
    EXPECT_TRUE(aw < bw);
    EXPECT_TRUE(aw <= bw);
    EXPECT_FALSE(aw > bw);
    EXPECT_FALSE(aw >= bw);

    EXPECT_FALSE(bw == aw);
    EXPECT_TRUE(bw != aw);
    EXPECT_FALSE(bw < aw);
    EXPECT_FALSE(bw <= aw);
    EXPECT_TRUE(bw > aw);
    EXPECT_TRUE(bw >= aw);
  }
}
