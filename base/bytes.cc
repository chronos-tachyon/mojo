// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/bytes.h"

namespace base {
namespace internal {

static constexpr std::size_t BITS = sizeof(std::size_t) * 8;

static constexpr std::size_t rotate(
    std::size_t x, unsigned int shift) noexcept {
  return ((x >> shift) | (x << (BITS - shift)));
}

std::size_t hash_bytes(const uint8_t* ptr, std::size_t len) noexcept {
  if (!len) return 0;
  const uint8_t* p = ptr;
  const uint8_t* q = ptr + len;
  const std::size_t mul = 7907U + len * 2U;
  std::size_t h = len * 3U;
  while (p != q) {
    h = rotate(h, 27) * mul + *p;
    ++p;
  }
  return h;
}

}  // namespace internal

template<> constexpr std::size_t BasicBytes<false>::npos;
template<> constexpr std::size_t BasicBytes<true>::npos;

}  // namespace base
