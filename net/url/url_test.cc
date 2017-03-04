#include "gtest/gtest.h"

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <tuple>

#include "base/concat.h"
#include "base/result_testing.h"
#include "net/url/url.h"

static std::ostream& operator<<(std::ostream& o, const net::url::URL& u) {
  return (o << u.as_string());
}

static bool eqv(const net::url::URL& a, const net::url::URL& b) {
  return a.equivalent_to(b);
}

TEST(URL, Parse) {
  constexpr auto EMPTY = base::StringPiece();
  using Pair = std::pair<bool, base::StringPiece>;
  using Triple = std::tuple<bool, base::StringPiece, base::StringPiece>;

  struct TestData {
    std::string input;
    bool via_request;
    Pair scheme;
    Pair opaque;
    Pair username;
    Pair password;
    Pair hostname;
    Triple path;
    Triple query;
    Triple fragment;
    std::string normalized;
  };

  std::vector<TestData> testdata = {
    {
      "*",
      true,
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Triple(true, "*", "*"),
      Triple(false, EMPTY, EMPTY),
      Triple(false, EMPTY, EMPTY),
      "*",
    },
    {
      "/uri",
      true,
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Triple(true, "/uri", "/uri"),
      Triple(false, EMPTY, EMPTY),
      Triple(false, EMPTY, EMPTY),
      "/uri",
    },
    {
      "///uri",
      true,
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Triple(true, "///uri", "///uri"),
      Triple(false, EMPTY, EMPTY),
      Triple(false, EMPTY, EMPTY),
      "///uri",
    },
    {
      "///uri",
      false,
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(true, ""),
      Triple(true, "/uri", "/uri"),
      Triple(false, EMPTY, EMPTY),
      Triple(false, EMPTY, EMPTY),
      "/uri",
    },
    {
      "mailto:user@example.com?subject=Hello,+world!",
      false,
      Pair(true, "mailto"),
      Pair(true, "user@example.com"),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Triple(false, EMPTY, EMPTY),
      Triple(true, "subject=Hello,+world!", "subject=Hello,+world!"),
      Triple(false, EMPTY, EMPTY),
      "mailto:user@example.com?subject=Hello,+world!",
    },
    {
      "http://user@example.com/foo?q=bar#baz",
      false,
      Pair(true, "http"),
      Pair(false, EMPTY),
      Pair(true, "user"),
      Pair(false, EMPTY),
      Pair(true, "example.com"),
      Triple(true, "/foo", "/foo"),
      Triple(true, "q=bar", "q=bar"),
      Triple(true, "baz", "baz"),
      "http://user@example.com/foo?q=bar#baz",
    },
    {
      "HTTPS://USER:PW@EXAMPLE.COM?Q=BAR#BAZ",
      false,
      Pair(true, "HTTPS"),
      Pair(false, EMPTY),
      Pair(true, "USER"),
      Pair(true, "PW"),
      Pair(true, "EXAMPLE.COM"),
      Triple(false, EMPTY, EMPTY),
      Triple(true, "Q=BAR", "Q=BAR"),
      Triple(true, "BAZ", "BAZ"),
      "https://USER:PW@example.com/?Q=BAR#BAZ",
    },
    {
      "file:///etc/passwd",
      false,
      Pair(true, "file"),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(true, ""),
      Triple(true, "/etc/passwd", "/etc/passwd"),
      Triple(false, EMPTY, EMPTY),
      Triple(false, EMPTY, EMPTY),
      "file:///etc/passwd",
    },
    {
      "http://example.edu/%7euser/",
      false,
      Pair(true, "http"),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(true, "example.edu"),
      Triple(true, "/%7euser/", "/~user/"),
      Triple(false, EMPTY, EMPTY),
      Triple(false, EMPTY, EMPTY),
      "http://example.edu/~user/",
    },
    {
      "http://example.org/#a%3db",
      false,
      Pair(true, "http"),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(true, "example.org"),
      Triple(true, "/", "/"),
      Triple(false, EMPTY, EMPTY),
      Triple(true, "a%3db", "a=b"),
      "http://example.org/#a=b",
    },
    {
      "//example.org",
      false,
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(true, "example.org"),
      Triple(false, EMPTY, EMPTY),
      Triple(false, EMPTY, EMPTY),
      Triple(false, EMPTY, EMPTY),
      "//example.org/",
    },
    {
      "http://example.org/?#",
      false,
      Pair(true, "http"),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(false, EMPTY),
      Pair(true, "example.org"),
      Triple(true, "/", "/"),
      Triple(true, "", ""),
      Triple(true, "", ""),
      "http://example.org/",
    },
  };

  for (const auto& row : testdata) {
    SCOPED_TRACE(base::concat(row.input, " - ", row.via_request));
    net::url::URL url;
    auto result = url.parse(row.input, row.via_request);
    EXPECT_OK(result);
    if (!result) continue;
    std::string qs = url.query().as_string();
    EXPECT_EQ(row.scheme, Pair(url.has_scheme(), url.scheme()));
    EXPECT_EQ(row.opaque, Pair(url.has_opaque(), url.opaque()));
    EXPECT_EQ(row.username, Pair(url.has_username(), url.username()));
    EXPECT_EQ(row.password, Pair(url.has_password(), url.password()));
    EXPECT_EQ(row.hostname, Pair(url.has_hostname(), url.hostname()));
    EXPECT_EQ(row.path, Triple(url.has_path(), url.raw_path(), url.path()));
    EXPECT_EQ(row.query, Triple(url.has_query(), url.raw_query(), qs));
    EXPECT_EQ(row.fragment, Triple(url.has_fragment(), url.raw_fragment(), url.fragment()));
    EXPECT_EQ(row.input, url.as_string());
    url.normalize();
    EXPECT_EQ(row.normalized, url.as_string());
  }
}

TEST(URL, EQAndEquivalentTo) {
  net::url::URL u1, u2;

  ASSERT_OK(u1.parse("http://example.com", false));
  ASSERT_OK(u2.parse("http://example.com", false));
  EXPECT_EQ(u1, u2);
  EXPECT_PRED2(eqv, u1, u2);

  ASSERT_OK(u1.parse("HTTP://EXAMPLE.COM", false));
  ASSERT_OK(u2.parse("http://example.com?", false));
  EXPECT_NE(u1, u2);
  EXPECT_PRED2(eqv, u1, u2);

  ASSERT_OK(u1.parse("http://example.com/#", false));
  ASSERT_OK(u2.parse("http://example.com", false));
  EXPECT_NE(u1, u2);
  EXPECT_PRED2(eqv, u1, u2);
}
