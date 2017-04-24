// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_PRIMITIVES_H
#define CRYPTO_PRIMITIVES_H

#include <cstdint>

namespace crypto {
namespace primitives {

constexpr bool X86OPT =
#if defined(__i386__) || defined(__x86_64__)
    true;
#else
    false;
#endif

constexpr inline uint32_t ROL32(uint32_t x, unsigned int c) {
  return (x << c) | (x >> (32 - c));
}

constexpr inline uint64_t ROL64(uint64_t x, unsigned int c) {
  return (x << c) | (x >> (64 - c));
}

constexpr inline uint32_t ROR32(uint32_t x, unsigned int c) {
  return (x >> c) | (x << (32 - c));
}

constexpr inline uint64_t ROR64(uint64_t x, unsigned int c) {
  return (x >> c) | (x << (64 - c));
}

inline uint32_t RLE32(const uint8_t* ptr, unsigned int index) {
  if (X86OPT) {
    const uint32_t* ptr32 = reinterpret_cast<const uint32_t*>(ptr);
    return ptr32[index];
  } else {
    uint32_t byte0 = ptr[(index * 4) + 0];
    uint32_t byte1 = ptr[(index * 4) + 1];
    uint32_t byte2 = ptr[(index * 4) + 2];
    uint32_t byte3 = ptr[(index * 4) + 3];
    return byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24);
  }
}

inline uint64_t RLE64(const uint8_t* ptr, unsigned int index) {
  if (X86OPT) {
    const uint64_t* ptr64 = reinterpret_cast<const uint64_t*>(ptr);
    return ptr64[index];
  } else {
    uint64_t byte0 = ptr[(index * 4) + 0];
    uint64_t byte1 = ptr[(index * 4) + 1];
    uint64_t byte2 = ptr[(index * 4) + 2];
    uint64_t byte3 = ptr[(index * 4) + 3];
    uint64_t byte4 = ptr[(index * 4) + 4];
    uint64_t byte5 = ptr[(index * 4) + 5];
    uint64_t byte6 = ptr[(index * 4) + 6];
    uint64_t byte7 = ptr[(index * 4) + 7];
    return byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24) | (byte4 << 32) | (byte5 << 40) | (byte6 << 48) | (byte7 << 56);
  }
}

inline uint32_t RBE32(const uint8_t* ptr, unsigned int index) {
  uint32_t byte0 = ptr[(index * 4) + 0];
  uint32_t byte1 = ptr[(index * 4) + 1];
  uint32_t byte2 = ptr[(index * 4) + 2];
  uint32_t byte3 = ptr[(index * 4) + 3];
  return (byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3;
}

inline uint64_t RBE64(const uint8_t* ptr, unsigned int index) {
  uint64_t byte0 = ptr[(index * 8) + 0];
  uint64_t byte1 = ptr[(index * 8) + 1];
  uint64_t byte2 = ptr[(index * 8) + 2];
  uint64_t byte3 = ptr[(index * 8) + 3];
  uint64_t byte4 = ptr[(index * 8) + 4];
  uint64_t byte5 = ptr[(index * 8) + 5];
  uint64_t byte6 = ptr[(index * 8) + 6];
  uint64_t byte7 = ptr[(index * 8) + 7];
  return (byte0 << 56) | (byte1 << 48) | (byte2 << 40) | (byte3 << 32) | (byte4 << 24) | (byte5 << 16) | (byte6 << 8) | byte7;
}

inline void WLE32(uint8_t* ptr, unsigned int index, uint32_t value) {
  if (X86OPT) {
    uint32_t* ptr32 = reinterpret_cast<uint32_t*>(ptr);
    ptr32[index] = value;
  } else {
    ptr[(index * 4) + 0] = value & 0xff;
    ptr[(index * 4) + 1] = (value >> 8) & 0xff;
    ptr[(index * 4) + 2] = (value >> 16) & 0xff;
    ptr[(index * 4) + 3] = (value >> 24) & 0xff;
  }
}

inline void WLE64(uint8_t* ptr, unsigned int index, uint64_t value) {
  if (X86OPT) {
    uint64_t* ptr64 = reinterpret_cast<uint64_t*>(ptr);
    ptr64[index] = value;
  } else {
    ptr[(index * 8) + 0] = value & 0xff;
    ptr[(index * 8) + 1] = (value >> 8) & 0xff;
    ptr[(index * 8) + 2] = (value >> 16) & 0xff;
    ptr[(index * 8) + 3] = (value >> 24) & 0xff;
    ptr[(index * 8) + 4] = (value >> 32) & 0xff;
    ptr[(index * 8) + 5] = (value >> 40) & 0xff;
    ptr[(index * 8) + 6] = (value >> 48) & 0xff;
    ptr[(index * 8) + 7] = (value >> 56) & 0xff;
  }
}

inline void WBE32(uint8_t* ptr, unsigned int index, uint32_t value) {
  ptr[(index * 4) + 0] = (value >> 24) & 0xff;
  ptr[(index * 4) + 1] = (value >> 16) & 0xff;
  ptr[(index * 4) + 2] = (value >> 8) & 0xff;
  ptr[(index * 4) + 3] = value & 0xff;
}

inline void WBE64(uint8_t* ptr, unsigned int index, uint64_t value) {
  ptr[(index * 8) + 0] = (value >> 56) & 0xff;
  ptr[(index * 8) + 1] = (value >> 48) & 0xff;
  ptr[(index * 8) + 2] = (value >> 40) & 0xff;
  ptr[(index * 8) + 3] = (value >> 32) & 0xff;
  ptr[(index * 8) + 4] = (value >> 24) & 0xff;
  ptr[(index * 8) + 5] = (value >> 16) & 0xff;
  ptr[(index * 8) + 6] = (value >> 8) & 0xff;
  ptr[(index * 8) + 7] = value & 0xff;
}

inline void memxor(uint8_t* dst, const uint8_t* x, const uint8_t* y, std::size_t len) noexcept {
  if (X86OPT) {
    while (len >= 8) {
      uint64_t* dst64 = reinterpret_cast<uint64_t*>(dst);
      const uint64_t* x64 = reinterpret_cast<const uint64_t*>(x);
      const uint64_t* y64 = reinterpret_cast<const uint64_t*>(y);
      *dst64 = *x64 ^ *y64;
      dst += 8;
      x += 8;
      y += 8;
      len -= 8;
    }
  }
  while (len) {
    *dst = *x ^ *y;
    ++dst;
    ++x;
    ++y;
    --len;
  }
}

}  // namespace primitives
}  // namespace crypto

#endif  // CRYPTO_PRIMITIVES_H
