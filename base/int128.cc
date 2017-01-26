#include "base/int128.h"

#include <ostream>

#include "base/logging.h"

namespace base {

UInt128 operator*(UInt128 a, UInt128 b) noexcept {
  if (a.is_zero() || b.is_zero()) return 0;
  UInt128 result;
  for (std::size_t i = 0; i < 128; ++i) {
    if (b & 1U) result += a;
    a <<= 1;
    b >>= 1;
  }
  return result;
}

std::pair<UInt128, UInt128> divmod(UInt128 a, UInt128 b) {
  if (b.is_zero()) throw std::domain_error("divide by zero");
  constexpr UInt128 one(1U);
  UInt128 quo;
  UInt128 rem;
  std::size_t i = 128;
  while (i > 0) {
    --i;
    rem <<= 1;
    if (a.bit(i)) rem |= one;
    if (rem >= b) {
      rem -= b;
      quo |= (one << i);
    }
  }
  return std::make_pair(quo, rem);
}

std::string UInt128::as_string(unsigned int radix) const {
  static constexpr char kDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

  CHECK_GE(radix, 2U);
  CHECK_LE(radix, 36U);

  std::string str;
  if (is_zero()) {
    str.push_back('0');
  } else {
    UInt128 x(*this);
    while (x) {
      auto pair = divmod(x, radix);
      str.push_back(kDigits[uint8_t(pair.second)]);
      x = pair.first;
    }
    std::size_t i, j;
    i = 0;
    j = str.size() - 1;
    while (i < j) {
      using std::swap;
      swap(str[i], str[j]);
      ++i, --j;
    }
  }
  str.shrink_to_fit();
  return str;
}

std::ostream& operator<<(std::ostream& o, UInt128 x) {
  if (o.flags() & std::ios_base::hex) {
    return (o << x.as_string(16));
  } else if (o.flags() & std::ios_base::oct) {
    return (o << x.as_string(8));
  } else {
    return (o << x.as_string());
  }
}

}  // namespace base
