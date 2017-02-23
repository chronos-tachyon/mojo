#include "gtest/gtest.h"

#include <algorithm>
#include <initializer_list>
#include <iostream>

#include "base/result_testing.h"
#include "net/url/query.h"
#include "net/url/url.h"

template <
    typename T,
    typename = typename std::enable_if<std::is_same<
        std::string, decltype(std::declval<T>().as_string())>::value>::type>
static std::ostream& operator<<(std::ostream& s, const T& o) {
  return (s << o.as_string());
}

static std::vector<base::StringPiece> strvec(
    std::initializer_list<base::StringPiece> in) {
  std::vector<base::StringPiece> out;
  out.reserve(in.size());
  std::copy(in.begin(), in.end(), std::back_inserter(out));
  return out;
}

static std::pair<bool, base::StringPiece> boolstr(bool a, base::StringPiece b) {
  return std::make_pair(a, b);
}

static bool eqv(const net::url::URL& a, const net::url::URL& b) {
  return a.equivalent_to(b);
}

TEST(Query, AsString) {
  net::url::Query q;
  q.add("c", "5");
  q.add("b", "2");
  q.add("a", "1");
  q.add("b", "3");
  EXPECT_EQ("a=1&b=2&b=3&c=5", q.as_string());

  q.remove("c");
  EXPECT_EQ("a=1&b=2&b=3", q.as_string());

  q.remove("c");
  EXPECT_EQ("a=1&b=2&b=3", q.as_string());

  q.set("b", strvec({"23", "42"}));
  EXPECT_EQ("a=1&b=23&b=42", q.as_string());

  q.set("b", strvec({}));
  EXPECT_EQ("a=1", q.as_string());

  q.remove("a");
  EXPECT_EQ("", q.as_string());

  q.add("x", "a=b");
  q.add("y", "c&d");
  EXPECT_EQ("x=a%3Db&y=c%26d", q.as_string());
}

TEST(Query, Parse) {
  net::url::Query q;

  ASSERT_OK(q.parse("a=1&b=2&c=3"));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(strvec({"a", "b", "c"}), q.keys());
  EXPECT_EQ(strvec({"1"}), q.get_all("a"));
  EXPECT_EQ(strvec({"2"}), q.get_all("b"));
  EXPECT_EQ(strvec({"3"}), q.get_all("c"));
  EXPECT_EQ("a=1&b=2&c=3", q.as_string());

  ASSERT_OK(q.parse("a=1;b=2;c=3"));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(strvec({"a", "b", "c"}), q.keys());
  EXPECT_EQ(strvec({"1"}), q.get_all("a"));
  EXPECT_EQ(strvec({"2"}), q.get_all("b"));
  EXPECT_EQ(strvec({"3"}), q.get_all("c"));
  EXPECT_EQ("a=1&b=2&c=3", q.as_string());

  ASSERT_OK(q.parse("x=foo&x=bar&x=baz"));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(strvec({"x"}), q.keys());
  EXPECT_EQ(strvec({"foo", "bar", "baz"}), q.get_all("x"));
  EXPECT_EQ(boolstr(true, "foo"), q.get("x"));
  EXPECT_EQ(boolstr(true, "baz"), q.get_last("x"));
  EXPECT_EQ("x=foo&x=bar&x=baz", q.as_string());

  ASSERT_OK(q.parse("c=3&b=2&a=1&b=4"));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(strvec({"a", "b", "c"}), q.keys());
  EXPECT_EQ(strvec({"1"}), q.get_all("a"));
  EXPECT_EQ(strvec({"2", "4"}), q.get_all("b"));
  EXPECT_EQ(strvec({"3"}), q.get_all("c"));
  EXPECT_EQ("a=1&b=2&b=4&c=3", q.as_string());

  ASSERT_OK(q.parse("&&&a=1&&&b=2&&&c=3&&&"));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(strvec({"a", "b", "c"}), q.keys());
  EXPECT_EQ(strvec({"1"}), q.get_all("a"));
  EXPECT_EQ(strvec({"2"}), q.get_all("b"));
  EXPECT_EQ(strvec({"3"}), q.get_all("c"));
  EXPECT_EQ("a=1&b=2&c=3", q.as_string());

  ASSERT_OK(q.parse("?a=1?b=2?c=3?"));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(strvec({"a", "b", "c"}), q.keys());
  EXPECT_EQ(strvec({"1"}), q.get_all("a"));
  EXPECT_EQ(strvec({"2"}), q.get_all("b"));
  EXPECT_EQ(strvec({"3"}), q.get_all("c"));
  EXPECT_EQ("a=1&b=2&c=3", q.as_string());

  ASSERT_OK(q.parse("foo&bar&z=1"));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(strvec({"", "z"}), q.keys());
  EXPECT_EQ(strvec({"foo", "bar"}), q.get_all(""));
  EXPECT_EQ(strvec({"1"}), q.get_all("z"));
  EXPECT_EQ("foo&bar&z=1", q.as_string());

  ASSERT_OK(q.parse("q=a=b"));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(strvec({"q"}), q.keys());
  EXPECT_EQ(strvec({"a=b"}), q.get_all("q"));
  EXPECT_EQ("q=a%3Db", q.as_string());

  ASSERT_OK(q.parse("q=a%3Db"));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(strvec({"q"}), q.keys());
  EXPECT_EQ(strvec({"a=b"}), q.get_all("q"));
  EXPECT_EQ("q=a%3Db", q.as_string());

  ASSERT_OK(q.parse("q=a+b"));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(strvec({"q"}), q.keys());
  EXPECT_EQ(strvec({"a b"}), q.get_all("q"));
  EXPECT_EQ("q=a+b", q.as_string());

  ASSERT_OK(q.parse("search+query"));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(strvec({""}), q.keys());
  EXPECT_EQ(strvec({"search query"}), q.get_all(""));
  EXPECT_EQ("search+query", q.as_string());
}

TEST(Query, EQ) {
  net::url::Query q1, q2;

  ASSERT_OK(q1.parse("a=1&b=2&c=3&b=4"));
  ASSERT_OK(q2.parse("b=2&b=4&c=3&a=1"));
  EXPECT_EQ(q1, q2);

  ASSERT_OK(q1.parse("a=1"));
  ASSERT_OK(q2.parse("a=2"));
  EXPECT_NE(q1, q2);

  ASSERT_OK(q1.parse("a=1&a=2"));
  ASSERT_OK(q2.parse("a=1&a=3"));
  EXPECT_NE(q1, q2);

  ASSERT_OK(q1.parse("a=1"));
  ASSERT_OK(q2.parse("a=1&a=2"));
  EXPECT_NE(q1, q2);

  ASSERT_OK(q1.parse("a=1"));
  ASSERT_OK(q2.parse("a=1&b=2"));
  EXPECT_NE(q1, q2);
}

TEST(URL, Parse) {
  net::url::URL url;

  ASSERT_OK(url.parse("mailto:user@example.com?subject=Hello,+world!", false));
  EXPECT_TRUE(url.has_scheme());
  EXPECT_EQ("mailto", url.scheme());
  EXPECT_TRUE(url.has_opaque());
  EXPECT_EQ("user@example.com", url.opaque());
  EXPECT_FALSE(url.has_username());
  EXPECT_FALSE(url.has_password());
  EXPECT_FALSE(url.has_hostname());
  EXPECT_FALSE(url.has_path());
  EXPECT_TRUE(url.has_query());
  EXPECT_EQ("subject=Hello,+world!", url.raw_query());
  EXPECT_FALSE(url.has_fragment());
  EXPECT_EQ("mailto:user@example.com?subject=Hello,+world!", url.as_string());
  url.normalize();
  EXPECT_EQ("mailto:user@example.com?subject=Hello,+world!", url.as_string());

  ASSERT_OK(url.parse("http://user@example.com/foo?q=bar#baz", false));
  EXPECT_TRUE(url.has_scheme());
  EXPECT_EQ("http", url.scheme());
  EXPECT_FALSE(url.has_opaque());
  EXPECT_TRUE(url.has_username());
  EXPECT_EQ("user", url.username());
  EXPECT_FALSE(url.has_password());
  EXPECT_TRUE(url.has_hostname());
  EXPECT_EQ("example.com", url.hostname());
  EXPECT_TRUE(url.has_path());
  EXPECT_EQ("/foo", url.raw_path());
  EXPECT_TRUE(url.has_query());
  EXPECT_EQ("q=bar", url.raw_query());
  EXPECT_TRUE(url.has_fragment());
  EXPECT_EQ("baz", url.fragment());
  EXPECT_EQ("http://user@example.com/foo?q=bar#baz", url.as_string());
  url.normalize();
  EXPECT_EQ("http://user@example.com/foo?q=bar#baz", url.as_string());

  ASSERT_OK(url.parse("HTTPS://USER:PW@EXAMPLE.COM?Q=BAR#BAZ", false));
  EXPECT_TRUE(url.has_scheme());
  EXPECT_EQ("HTTPS", url.scheme());
  EXPECT_FALSE(url.has_opaque());
  EXPECT_TRUE(url.has_username());
  EXPECT_EQ("USER", url.username());
  EXPECT_TRUE(url.has_password());
  EXPECT_EQ("PW", url.password());
  EXPECT_TRUE(url.has_hostname());
  EXPECT_EQ("EXAMPLE.COM", url.hostname());
  EXPECT_FALSE(url.has_path());
  EXPECT_TRUE(url.has_query());
  EXPECT_EQ("Q=BAR", url.raw_query());
  EXPECT_TRUE(url.has_fragment());
  EXPECT_EQ("BAZ", url.fragment());
  EXPECT_EQ("HTTPS://USER:PW@EXAMPLE.COM?Q=BAR#BAZ", url.as_string());
  url.normalize();
  EXPECT_EQ("https", url.scheme());
  EXPECT_EQ("example.com", url.hostname());
  EXPECT_TRUE(url.has_path());
  EXPECT_EQ("/", url.raw_path());
  EXPECT_EQ("https://USER:PW@example.com/?Q=BAR#BAZ", url.as_string());

  ASSERT_OK(url.parse("file:///etc/passwd", false));
  EXPECT_TRUE(url.has_scheme());
  EXPECT_EQ("file", url.scheme());
  EXPECT_FALSE(url.has_opaque());
  EXPECT_FALSE(url.has_username());
  EXPECT_FALSE(url.has_password());
  EXPECT_FALSE(url.has_hostname());
  EXPECT_TRUE(url.has_path());
  EXPECT_EQ("/etc/passwd", url.raw_path());
  EXPECT_FALSE(url.has_query());
  EXPECT_FALSE(url.has_fragment());
  EXPECT_EQ("file:///etc/passwd", url.as_string());
  url.normalize();
  EXPECT_EQ("file:///etc/passwd", url.as_string());

  ASSERT_OK(url.parse("http://example.edu/%7euser/", false));
  EXPECT_TRUE(url.has_scheme());
  EXPECT_EQ("http", url.scheme());
  EXPECT_FALSE(url.has_opaque());
  EXPECT_FALSE(url.has_username());
  EXPECT_FALSE(url.has_password());
  EXPECT_TRUE(url.has_hostname());
  EXPECT_EQ("example.edu", url.hostname());
  EXPECT_TRUE(url.has_path());
  EXPECT_EQ("/~user/", url.path());
  EXPECT_EQ("/%7euser/", url.raw_path());
  EXPECT_FALSE(url.has_query());
  EXPECT_FALSE(url.has_fragment());
  EXPECT_EQ("http://example.edu/%7euser/", url.as_string());
  url.normalize();
  EXPECT_EQ("http://example.edu/~user/", url.as_string());

  ASSERT_OK(url.parse("http://example.org/#a%3db", false));
  EXPECT_TRUE(url.has_scheme());
  EXPECT_EQ("http", url.scheme());
  EXPECT_FALSE(url.has_opaque());
  EXPECT_FALSE(url.has_username());
  EXPECT_FALSE(url.has_password());
  EXPECT_TRUE(url.has_hostname());
  EXPECT_EQ("example.org", url.hostname());
  EXPECT_TRUE(url.has_path());
  EXPECT_EQ("/", url.raw_path());
  EXPECT_FALSE(url.has_query());
  EXPECT_TRUE(url.has_fragment());
  EXPECT_EQ("a=b", url.fragment());
  EXPECT_EQ("a%3db", url.raw_fragment());
  EXPECT_EQ("http://example.org/#a%3db", url.as_string());
  url.normalize();
  EXPECT_EQ("http://example.org/#a=b", url.as_string());

  ASSERT_OK(url.parse("//example.org", false));
  EXPECT_FALSE(url.has_scheme());
  EXPECT_FALSE(url.has_opaque());
  EXPECT_FALSE(url.has_username());
  EXPECT_FALSE(url.has_password());
  EXPECT_TRUE(url.has_hostname());
  EXPECT_EQ("example.org", url.hostname());
  EXPECT_FALSE(url.has_path());
  EXPECT_FALSE(url.has_query());
  EXPECT_FALSE(url.has_fragment());
  EXPECT_EQ("//example.org", url.as_string());
  url.normalize();
  EXPECT_TRUE(url.has_path());
  EXPECT_EQ("/", url.path());
  EXPECT_EQ("/", url.raw_path());
  EXPECT_EQ("//example.org/", url.as_string());

  ASSERT_OK(url.parse("http://example.org/?#", false));
  EXPECT_TRUE(url.has_scheme());
  EXPECT_EQ("http", url.scheme());
  EXPECT_FALSE(url.has_opaque());
  EXPECT_FALSE(url.has_username());
  EXPECT_FALSE(url.has_password());
  EXPECT_TRUE(url.has_hostname());
  EXPECT_EQ("example.org", url.hostname());
  EXPECT_TRUE(url.has_path());
  EXPECT_EQ("/", url.raw_path());
  EXPECT_TRUE(url.has_query());
  EXPECT_EQ("", url.raw_query());
  EXPECT_TRUE(url.has_fragment());
  EXPECT_EQ("", url.raw_fragment());
  EXPECT_EQ("http://example.org/?#", url.as_string());
  url.normalize();
  EXPECT_FALSE(url.has_query());
  EXPECT_FALSE(url.has_fragment());
  EXPECT_EQ("http://example.org/", url.as_string());
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
