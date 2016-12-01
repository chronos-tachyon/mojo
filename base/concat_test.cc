// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <map>
#include <vector>

#include "base/concat.h"

TEST(StrCat, Basic) {
  EXPECT_EQ("", base::concat());
  EXPECT_EQ("abc", base::concat('a', 'b', 'c'));
  EXPECT_EQ("abcdef", base::concat("abc", "def"));
  EXPECT_EQ("abc123", base::concat("abc", 123));
  EXPECT_EQ("123abc", base::concat(123, "abc"));
  EXPECT_EQ("123456", base::concat(123, 456));
  EXPECT_EQ("truefalse", base::concat(true, false));

  const char chararray[] = "hello";
  EXPECT_EQ("hello", base::concat(chararray));

  const char* charptr = "goodbye";
  EXPECT_EQ("goodbye", base::concat(charptr));

  std::string str = "whattup?";
  EXPECT_EQ("whattup?", base::concat(str));
}

struct Foo {
  void append_to(std::string& out) const { out.append("foo"); }
};

struct Bar {
  void append_to(std::string& out) const { out.append("bar"); }
  std::size_t length_hint() const { return 3; }
};

TEST(StrCat, Methods) {
  auto str = base::concat(Foo());
  EXPECT_EQ("foo", str);
  str = base::concat(Bar());
  EXPECT_EQ("bar", str);
  auto hint = base::length_hint(Foo());
  EXPECT_EQ(0U, hint);
  hint = base::length_hint(Bar());
  EXPECT_EQ(3U, hint);
}

TEST(StrCat, Pairs) {
  auto str = base::concat(std::make_pair(2, 3));
  EXPECT_EQ("<2, 3>", str);
  str = base::concat(std::make_pair("foo", false));
  EXPECT_EQ("<foo, false>", str);
}

TEST(StrCat, Tuples) {
  auto str = base::concat(std::make_tuple());
  EXPECT_EQ("<>", str);
  str = base::concat(std::make_tuple(5));
  EXPECT_EQ("<5>", str);
  str = base::concat(std::make_tuple(false, 42, "foo"));
  EXPECT_EQ("<false, 42, foo>", str);
}

TEST(StrCat, Vector) {
  std::vector<int> vec = {2, 3, 5};
  auto str = base::concat(vec);
  EXPECT_EQ("[2, 3, 5]", str);
}

TEST(StrCat, Map) {
  std::map<int, int> map = {{2, 4}, {3, 9}, {5, 25}};
  auto str = base::concat(map);
  EXPECT_EQ("[<2, 4>, <3, 9>, <5, 25>]", str);
}
