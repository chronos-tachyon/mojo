#include "gtest/gtest.h"

#include <climits>
#include <ostream>

#include "base/safemath.h"

static_assert(SCHAR_MIN < -SCHAR_MAX, "this test assumes 2's-complement arithmetic");
static_assert(SCHAR_MAX == 127, "this test assumes 8-bit bytes");
static_assert(UCHAR_MAX == 255U, "this test assumes 8-bit bytes");

namespace base {
static std::ostream& operator<<(std::ostream& o, safe<unsigned char> x) {
  return (o << "safe(" << uint16_t(x) << ")");
}
static std::ostream& operator<<(std::ostream& o, safe<signed char> x) {
  return (o << "safe(" << int16_t(x) << ")");
}
}

TEST(SafeUnsigned, Add) {
  using Safe = base::safe<unsigned char>;

  EXPECT_EQ(Safe(0U), Safe(0U) + Safe(0U));
  EXPECT_EQ(Safe(1U), Safe(1U) + Safe(0U));
  EXPECT_EQ(Safe(2U), Safe(1U) + Safe(1U));

  EXPECT_EQ(Safe(255U), Safe(0U) + Safe(255U));
  EXPECT_EQ(Safe(255U), Safe(1U) + Safe(254U));
  EXPECT_EQ(Safe(255U), Safe(127U) + Safe(128U));
  EXPECT_EQ(Safe(255U), Safe(128U) + Safe(127U));
  EXPECT_EQ(Safe(255U), Safe(254U) + Safe(1U));
  EXPECT_EQ(Safe(255U), Safe(255U) + Safe(0U));

  EXPECT_THROW(Safe(128U) + Safe(128U), std::overflow_error);
  EXPECT_THROW(Safe(254U) + Safe(2U), std::overflow_error);
  EXPECT_THROW(Safe(255U) + Safe(1U), std::overflow_error);
}

TEST(SafeUnsigned, Subtract) {
  using Safe = base::safe<unsigned char>;

  EXPECT_EQ(Safe(0U), Safe(1U) - Safe(1U));
  EXPECT_EQ(Safe(1U), Safe(2U) - Safe(1U));
  EXPECT_THROW(Safe(1U) - Safe(2U), std::overflow_error);
}

TEST(SafeUnsigned, Multiply) {
  using Safe = base::safe<unsigned char>;

  EXPECT_EQ(Safe(0U), Safe(255U) * Safe(0U));
  EXPECT_EQ(Safe(0U), Safe(0U) * Safe(255U));

  EXPECT_EQ(Safe(255U), Safe(255U) * Safe(1U));
  EXPECT_EQ(Safe(255U), Safe(1U) * Safe(255U));

  EXPECT_EQ(Safe(240U), Safe(16U) * Safe(15U));
  EXPECT_EQ(Safe(240U), Safe(15U) * Safe(16U));

  EXPECT_THROW(Safe(16U) * Safe(16U), std::overflow_error);
  EXPECT_THROW(Safe(128U) * Safe(2U), std::overflow_error);
  EXPECT_THROW(Safe(2U) * Safe(128U), std::overflow_error);
}

TEST(SafeUnsigned, Divide) {
  using Safe = base::safe<unsigned char>;

  EXPECT_EQ(Safe(15U), Safe(240U) / Safe(16U));
  EXPECT_EQ(Safe(255U), Safe(255U) / Safe(1U));
  EXPECT_THROW(Safe(255U) / Safe(0U), std::domain_error);

  EXPECT_EQ(Safe(15U), Safe(255U) % Safe(16U));
  EXPECT_EQ(Safe(9U), Safe(249U) % Safe(16U));
  EXPECT_EQ(Safe(1U), Safe(255U) % Safe(2U));
  EXPECT_EQ(Safe(0U), Safe(255U) % Safe(1U));
  EXPECT_THROW(Safe(255U) % Safe(0U), std::domain_error);
}

TEST(SafeSigned, Negate) {
  using Safe = base::safe<signed char>;

  EXPECT_EQ(Safe(-127), -Safe(+127));
  EXPECT_EQ(Safe(-1), -Safe(+1));
  EXPECT_EQ(Safe(+0), -Safe(+0));
  EXPECT_EQ(Safe(+1), -Safe(-1));
  EXPECT_EQ(Safe(+127), -Safe(-127));
  EXPECT_THROW(-Safe(-128), std::overflow_error);
}

TEST(SafeSigned, Add) {
  using Safe = base::safe<signed char>;

  EXPECT_EQ(Safe(0), Safe(0) + Safe(0));
  EXPECT_EQ(Safe(1), Safe(1) + Safe(0));
  EXPECT_EQ(Safe(2), Safe(1) + Safe(1));
  EXPECT_EQ(Safe(1), Safe(2) - Safe(1));
  EXPECT_EQ(Safe(0), Safe(2) - Safe(2));
  EXPECT_EQ(Safe(-1), Safe(2) - Safe(3));

  EXPECT_EQ(Safe(127), Safe(64) + Safe(63));
  EXPECT_THROW(Safe(64) + Safe(64), std::overflow_error);
  EXPECT_THROW(Safe(127) + Safe(1), std::overflow_error);

  EXPECT_EQ(Safe(-128), Safe(+0) + Safe(-128));
  EXPECT_EQ(Safe(-128), Safe(-1) + Safe(-127));
  EXPECT_EQ(Safe(-128), Safe(-64) + Safe(-64));
  EXPECT_EQ(Safe(-128), Safe(-127) + Safe(-1));
  EXPECT_EQ(Safe(-128), Safe(-128) + Safe(+0));
  EXPECT_THROW(Safe(-128) + Safe(-1), std::overflow_error);
}

TEST(SafeSigned, Subtract) {
  using Safe = base::safe<signed char>;

  EXPECT_THROW(Safe(-2) - Safe(127), std::overflow_error);
  EXPECT_EQ(Safe(-128), Safe(-1) - Safe(127));
  EXPECT_EQ(Safe(-128), Safe(-127) - Safe(1));
  EXPECT_THROW(Safe(-127) - Safe(2), std::overflow_error);
  EXPECT_THROW(Safe(-128) - Safe(1), std::overflow_error);
}

TEST(SafeSigned, Multiply) {
  using Safe = base::safe<signed char>;

  EXPECT_EQ(Safe(16), Safe(4) * Safe(4));
  EXPECT_EQ(Safe(-16), Safe(-4) * Safe(4));
  EXPECT_EQ(Safe(-16), Safe(4) * Safe(-4));
  EXPECT_EQ(Safe(16), Safe(-4) * Safe(-4));

  EXPECT_EQ(Safe(112), Safe(16) * Safe(7));
  EXPECT_EQ(Safe(-112), Safe(-16) * Safe(7));
  EXPECT_EQ(Safe(-112), Safe(16) * Safe(-7));
  EXPECT_EQ(Safe(112), Safe(-16) * Safe(-7));

  EXPECT_THROW(Safe(16) * Safe(8), std::overflow_error);
  EXPECT_EQ(Safe(-128), Safe(-16) * Safe(8));
  EXPECT_EQ(Safe(-128), Safe(16) * Safe(-8));
  EXPECT_THROW(Safe(-16) * Safe(-8), std::overflow_error);

  EXPECT_THROW(Safe(-128) * Safe(-1), std::overflow_error);
}

TEST(SafeSigned, Divide) {
  using Safe = base::safe<signed char>;

  EXPECT_EQ(Safe(-8), Safe(-128) / Safe(16));
  EXPECT_EQ(Safe(-7), Safe(112) / Safe(-16));
  EXPECT_THROW(Safe(-128) / Safe(-1), std::overflow_error);
  EXPECT_THROW(Safe(-128) / Safe(0), std::domain_error);

  EXPECT_EQ(Safe(15), Safe(127) % Safe(16));
  EXPECT_EQ(Safe(9), Safe(121) % Safe(16));
  EXPECT_EQ(Safe(1), Safe(113) % Safe(2));
  EXPECT_EQ(Safe(0), Safe(112) % Safe(1));
  EXPECT_THROW(Safe(127) % Safe(0), std::domain_error);
}
