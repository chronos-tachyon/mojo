#include "gtest/gtest.h"

#include <algorithm>
#include <initializer_list>
#include <iostream>

#include "base/result_testing.h"
#include "net/url/query.h"

static std::ostream& operator<<(std::ostream& o, const net::url::Query& q) {
  return (o << q.as_string());
}

static std::vector<base::StringPiece> strvec(
    std::initializer_list<base::StringPiece> il) {
  return std::vector<base::StringPiece>(il);
}

static std::pair<bool, base::StringPiece> boolstr(bool a, base::StringPiece b) {
  return std::make_pair(a, b);
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
