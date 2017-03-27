// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "encoding/base64.h"

#include <cstring>

#include "base/logging.h"

static constexpr uint8_t NPOS = 0xff;

static uint8_t find(const char* cs, char ch) {
  uint8_t index = 0;
  while (*cs) {
    if (*cs == ch) return index;
    ++cs;
    ++index;
  }
  return NPOS;
}

static bool is_space(char ch) { return find(" \t\r\n\f\v", ch) != NPOS; }

namespace encoding {

std::size_t encoded_length(Base64 b64, std::size_t len) noexcept {
  std::size_t sum = (len / 3) * 4;
  len %= 3;
  if (len) {
    if (b64.pad) {
      sum += 4;
    } else if (len == 2) {
      sum += 3;
    } else if (len == 1) {
      sum += 2;
    }
  }
  return sum;
}

static constexpr inline uint32_t X0(const uint8_t* ptr,
                                    unsigned int index) noexcept {
  return uint32_t(ptr[index]) << (16 - (index * 8));
}

std::size_t encode_to(Base64 b64, char* dst, const uint8_t* src,
                      std::size_t len) noexcept {
  std::size_t n = 0;

  while (len >= 3) {
    uint32_t word = X0(src, 0) | X0(src, 1) | X0(src, 2);
    dst[0] = b64.charset[(word >> 18) & 63];
    dst[1] = b64.charset[(word >> 12) & 63];
    dst[2] = b64.charset[(word >> 6) & 63];
    dst[3] = b64.charset[word & 63];
    dst += 4;
    n += 4;
    src += 3;
    len -= 3;
  }

  if (len == 2) {
    uint32_t word = X0(src, 0) | X0(src, 1);
    dst[0] = b64.charset[(word >> 18) & 63];
    dst[1] = b64.charset[(word >> 12) & 63];
    dst[2] = b64.charset[(word >> 6) & 63];
    if (b64.pad) {
      dst[3] = b64.charset[64];
      n += 4;
    } else {
      n += 3;
    }
  } else if (len == 1) {
    uint32_t word = X0(src, 0);
    dst[0] = b64.charset[(word >> 18) & 63];
    dst[1] = b64.charset[(word >> 12) & 63];
    if (b64.pad) {
      dst[2] = b64.charset[64];
      dst[3] = b64.charset[64];
      n += 4;
    } else {
      n += 2;
    }
  }

  return n;
}

std::string encode(Base64 b64, const uint8_t* src, std::size_t len) {
  std::vector<char> tmp;
  tmp.resize(encoded_length(b64, len));
  auto n = encode_to(b64, tmp.data(), src, len);
  return std::string(tmp.data(), n);
}

std::size_t decoded_length(Base64, std::size_t len) noexcept {
  return ((len + 3) / 4) * 3;
}

static inline std::size_t Y(uint8_t* out, const uint8_t* ptr, unsigned int num) noexcept {
  uint32_t val = 0;
  for (unsigned int i = 0; i < num; ++i) {
    val |= uint32_t(ptr[i]) << (18 - (i * 6));
  }
  switch (num) {
    case 4:
      out[0] = (val >> 16) & 0xff;
      out[1] = (val >> 8) & 0xff;
      out[2] = (val >> 0) & 0xff;
      return 3;

    case 3:
      out[0] = (val >> 16) & 0xff;
      out[1] = (val >> 8) & 0xff;
      return 2;

    case 2:
    case 1:
      out[0] = (val >> 16) & 0xff;
      return 1;
  }
  return 0;
}

std::pair<bool, std::size_t> decode_to(Base64 b64, uint8_t* dst,
                                       const char* src,
                                       std::size_t len) noexcept {
  std::size_t n = 0;
  uint8_t x[4];
  uint8_t ax = 0, bx = 0;
  bool got_pad = false;
  bool used_pad = false;

  while (len) {
    char ch = src[0];
    ++src;
    --len;

    auto val = find(b64.charset, ch);
    if (val < 64) {
      if (got_pad) return std::make_pair(false, 0);
      x[ax++] = val;
      bx++;
    } else if (val == 64) {
      got_pad = true;
      x[ax++] = 0;
    } else if (is_space(ch)) {
      continue;
    } else {
      return std::make_pair(false, 0);
    }
    if (used_pad) return std::make_pair(false, 0);

    if (ax == 4) {
      auto num = Y(dst, x, bx);
      dst += num;
      n += num;
      ax = bx = 0;
      if (got_pad) used_pad = true;
    }
  }

  if (bx) {
    while (ax < 4) x[ax++] = 0;
    auto num = Y(dst, x, bx);
    n += num;
  }

  return std::make_pair(true, n);
}

std::pair<bool, std::string> decode(Base64 b64, const char* src,
                                    std::size_t len) {
  std::vector<uint8_t> tmp;
  tmp.resize(decoded_length(b64, len));
  auto pair = decode_to(b64, tmp.data(), src, len);
  if (pair.first) {
    auto* ptr = reinterpret_cast<char*>(tmp.data());
    auto len = pair.second;
    return std::make_pair(true, std::string(ptr, len));
  } else {
    return std::make_pair(false, std::string());
  }
}

}  // namespace encoding
