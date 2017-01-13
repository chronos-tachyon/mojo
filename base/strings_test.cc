#include "gtest/gtest.h"

#include "base/strings.h"

using SP = base::StringPiece;

TEST(StringPiece, Construct) {
  constexpr const char* kHello = "Hello!";

  constexpr SP kEmpty;
  static_assert(kEmpty.size() == 0, "");
  static_assert(kEmpty.empty(), "");

  constexpr SP kEmptyCopy(kEmpty);
  static_assert(kEmptyCopy.data() == kEmpty.data(), "");
  static_assert(kEmptyCopy.size() == 0, "");
  static_assert(kEmptyCopy.empty(), "");

  constexpr SP kPtrLen(kHello, 6);
  static_assert(kPtrLen.data() == kHello, "");
  static_assert(kPtrLen.size() == 6, "");
  static_assert(!kPtrLen.empty(), "");

  constexpr SP kPtr(kHello);
  static_assert(kPtr.data() == kHello, "");
  static_assert(kPtr.size() == 6, "");
  static_assert(!kPtr.empty(), "");

  constexpr SP kConst("Hello!");
  static_assert(kConst.data() != nullptr, "");
  static_assert(kConst.size() == 6, "");
  static_assert(!kConst.empty(), "");
  static_assert(kConst.front() == 'H', "");
  static_assert(kConst.back() == '!', "");
  static_assert(kConst[1] == 'e', "");

  std::string str("Hello!");
  const SP strsp(str);
  EXPECT_EQ(str.data(), strsp.data());
  EXPECT_EQ(str.size(), strsp.size());
  EXPECT_FALSE(strsp.empty());

  std::vector<char> vec;
  vec.insert(vec.end(), str.begin(), str.end());
  const SP vecsp(vec);
  EXPECT_EQ(vec.data(), vecsp.data());
  EXPECT_EQ(vec.size(), vecsp.size());
  EXPECT_FALSE(vecsp.empty());
}

TEST(StringPiece, Compare) {
  constexpr SP f = "f";
  constexpr SP foo = "foo";
  constexpr SP fooo = "fooo";
  constexpr SP fop = "fop";
  constexpr SP g = "g";

#define LT(a, b) \
  static_assert(a.compare(b) < 0, ""); \
  static_assert(a != b, ""); \
  static_assert(a < b, ""); \
  static_assert(a <= b, ""); \

#define EQ(a, b) \
  static_assert(a.compare(b) == 0, ""); \
  static_assert(a == b, ""); \
  static_assert(a <= b, ""); \
  static_assert(a >= b, "");

#define GT(a, b) \
  static_assert(a.compare(b) > 0, ""); \
  static_assert(a != b, ""); \
  static_assert(a > b, ""); \
  static_assert(a >= b, ""); \

  EQ(f, f);

  LT(f, foo);
  EQ(foo, foo);
  GT(foo, f);

  LT(f, fooo);
  LT(foo, fooo);
  EQ(fooo, fooo);
  GT(fooo, foo);
  GT(fooo, f);

  LT(f, fop);
  LT(foo, fop);
  LT(fooo, fop);
  EQ(fop, fop);
  GT(fop, fooo);
  GT(fop, foo);
  GT(fop, f);

  LT(f, g);
  LT(foo, g);
  LT(fooo, g);
  LT(fop, g);
  EQ(g, g);
  GT(g, fop);
  GT(g, fooo);
  GT(g, foo);
  GT(g, f);

#undef GT
#undef EQ
#undef LT
}

TEST(StringPiece, Substring) {
  constexpr SP foo = "abcdefghi";
  static_assert(foo.substring(0, 3) == "abc", "");
  static_assert(foo.substring(3, 3) == "def", "");
  static_assert(foo.substring(6, 3) == "ghi", "");
  static_assert(foo.substring(8, 3) == "i", "");
  static_assert(foo.substring(9, 3) == "", "");
  static_assert(foo.substring(10, 3) == "", "");

  static_assert(foo.substring(0) == "abcdefghi", "");
  static_assert(foo.substring(3) == "defghi", "");
  static_assert(foo.substring(6) == "ghi", "");
  static_assert(foo.substring(8) == "i", "");
  static_assert(foo.substring(9) == "", "");
  static_assert(foo.substring(10) == "", "");

  static_assert(foo.prefix(0) == "", "");
  static_assert(foo.prefix(1) == "a", "");
  static_assert(foo.prefix(3) == "abc", "");

  static_assert(foo.suffix(0) == "", "");
  static_assert(foo.suffix(1) == "i", "");
  static_assert(foo.suffix(3) == "ghi", "");

  static_assert(foo.has_prefix(""), "");
  static_assert(foo.has_prefix("a"), "");
  static_assert(foo.has_prefix("abc"), "");
  static_assert(!foo.has_prefix("x"), "");

  static_assert(foo.has_suffix(""), "");
  static_assert(foo.has_suffix("i"), "");
  static_assert(foo.has_suffix("ghi"), "");
  static_assert(!foo.has_suffix("x"), "");

  static_assert(base::remove_prefix(foo, 0) == "abcdefghi", "");
  static_assert(base::remove_prefix(foo, 1) == "bcdefghi", "");
  static_assert(base::remove_prefix(foo, 3) == "defghi", "");
  static_assert(base::remove_prefix(foo, 8) == "i", "");
  static_assert(base::remove_prefix(foo, 9) == "", "");
  static_assert(base::remove_prefix(foo, 10) == "", "");

  static_assert(base::remove_prefix(foo, "") == "abcdefghi", "");
  static_assert(base::remove_prefix(foo, "a") == "bcdefghi", "");
  static_assert(base::remove_prefix(foo, "abc") == "defghi", "");
  static_assert(base::remove_prefix(foo, "x") == "abcdefghi", "");

  static_assert(base::remove_suffix(foo, 0) == "abcdefghi", "");
  static_assert(base::remove_suffix(foo, 1) == "abcdefgh", "");
  static_assert(base::remove_suffix(foo, 3) == "abcdef", "");
  static_assert(base::remove_suffix(foo, 8) == "a", "");
  static_assert(base::remove_suffix(foo, 9) == "", "");
  static_assert(base::remove_suffix(foo, 10) == "", "");

  static_assert(base::remove_suffix(foo, "") == "abcdefghi", "");
  static_assert(base::remove_suffix(foo, "i") == "abcdefgh", "");
  static_assert(base::remove_suffix(foo, "ghi") == "abcdef", "");
  static_assert(base::remove_suffix(foo, "x") == "abcdefghi", "");

  SP sp = foo;
  EXPECT_FALSE(sp.remove_prefix("xyz"));
  EXPECT_EQ("abcdefghi", sp);

  sp = foo;
  EXPECT_TRUE(sp.remove_prefix("abc"));
  EXPECT_EQ("defghi", sp);

  sp = foo;
  sp.remove_prefix(3);
  EXPECT_EQ("defghi", sp);

  sp = foo;
  sp.remove_prefix(100);
  EXPECT_EQ("", sp);

  sp = foo;
  EXPECT_FALSE(sp.remove_suffix("xyz"));
  EXPECT_EQ("abcdefghi", sp);

  sp = foo;
  EXPECT_TRUE(sp.remove_suffix("ghi"));
  EXPECT_EQ("abcdef", sp);

  sp = foo;
  sp.remove_suffix(3);
  EXPECT_EQ("abcdef", sp);

  sp = foo;
  sp.remove_suffix(100);
  EXPECT_EQ("", sp);
}
