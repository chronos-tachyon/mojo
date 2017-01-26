#include "gtest/gtest.h"

#include "base/int128.h"

using U = base::UInt128;

TEST(UInt128, Basics) {
  const U zero;
  EXPECT_EQ(U::min(), zero);

  U x;
  --x;
  EXPECT_EQ(U::max(), x);

  x = U(1U, 0U);
  --x;
  EXPECT_EQ(U(0ULL, ~0ULL), x);
}

TEST(UInt128, DivMod) {
  const U zero;
  const U two(2);
  const U four(4);
  const U ten(10);
  const U fortytwo(42);
  U quo, rem;

  std::tie(quo, rem) = divmod(zero, ten);
  EXPECT_EQ(zero, quo);
  EXPECT_EQ(zero, rem);

  std::tie(quo, rem) = divmod(fortytwo, ten);
  EXPECT_EQ(four, quo);
  EXPECT_EQ(two, rem);
}

TEST(UInt128, AsString) {
  const U zero;
  EXPECT_EQ("0", zero.as_string());
  EXPECT_EQ("0", zero.as_string(2));
  EXPECT_EQ("0", zero.as_string(8));
  EXPECT_EQ("0", zero.as_string(10));
  EXPECT_EQ("0", zero.as_string(16));

  const U fortytwo(42);
  EXPECT_EQ("42", fortytwo.as_string());
  EXPECT_EQ("101010", fortytwo.as_string(2));
  EXPECT_EQ("52", fortytwo.as_string(8));
  EXPECT_EQ("42", fortytwo.as_string(10));
  EXPECT_EQ("2a", fortytwo.as_string(16));
}
