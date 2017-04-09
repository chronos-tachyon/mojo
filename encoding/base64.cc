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

std::size_t encode_to(Base64 b64, base::MutableChars dst,
                      base::Bytes src) noexcept {
  CHECK_GE(dst.size(), encoded_length(b64, src.size()));

  std::size_t i = 0, j = 0;
  std::size_t n = (src.size() / 3) * 3;
  while (i < n) {
    uint32_t word = (uint32_t(src[i + 0]) << 16) | (uint32_t(src[i + 1]) << 8) |
                    uint32_t(src[i + 2]);
    dst[j + 0] = b64.charset[(word >> 18) & 63];
    dst[j + 1] = b64.charset[(word >> 12) & 63];
    dst[j + 2] = b64.charset[(word >> 6) & 63];
    dst[j + 3] = b64.charset[word & 63];
    i += 3;
    j += 4;
  }

  std::size_t remaining = src.size() - i;
  if (remaining == 2) {
    uint32_t word = (uint32_t(src[i + 0]) << 16) | (uint32_t(src[i + 1]) << 8);
    dst[j + 0] = b64.charset[(word >> 18) & 63];
    dst[j + 1] = b64.charset[(word >> 12) & 63];
    dst[j + 2] = b64.charset[(word >> 6) & 63];
    if (b64.pad) {
      dst[j + 3] = b64.charset[64];
      j += 4;
    } else {
      j += 3;
    }
  } else if (remaining == 1) {
    uint32_t word = (uint32_t(src[i + 0]) << 16);
    dst[j + 0] = b64.charset[(word >> 18) & 63];
    dst[j + 1] = b64.charset[(word >> 12) & 63];
    if (b64.pad) {
      dst[j + 2] = b64.charset[64];
      dst[j + 3] = b64.charset[64];
      j += 4;
    } else {
      j += 2;
    }
  }

  return j;
}

std::string encode(Base64 b64, base::Bytes src) {
  std::vector<char> tmp;
  tmp.resize(encoded_length(b64, src.size()));
  auto len = encode_to(b64, tmp, src);
  return std::string(tmp.data(), len);
}

std::size_t decoded_length(Base64, std::size_t len) noexcept {
  return ((len + 3) / 4) * 3;
}

static inline std::size_t Y(uint8_t* out, const uint8_t* ptr,
                            unsigned int num) noexcept {
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

std::pair<bool, std::size_t> decode_to(Base64 b64, base::MutableBytes dst,
                                       base::Chars src) noexcept {
  CHECK_GE(dst.size(), decoded_length(b64, src.size()));
  std::size_t i = 0, j = 0;
  uint8_t x[4];
  uint8_t ax = 0, bx = 0;
  bool got_pad = false;
  while (i < src.size()) {
    char ch = src[i++];
    auto val = find(b64.charset, ch);
    if (val < 64) {
      if (got_pad) return std::make_pair(false, 0);
      x[ax] = val;
      ++ax, ++bx;
    } else if (val == 64) {
      got_pad = true;
      x[ax] = 0;
      ++ax;
    } else if (is_space(ch)) {
      continue;
    } else {
      return std::make_pair(false, 0);
    }

    if (ax == 4) {
      auto num = Y(dst.data() + j, x, bx);
      j += num;
      ax = bx = 0;
      if (got_pad) break;
    }
  }

  while (i < src.size()) {
    char ch = src[i++];
    if (!is_space(ch)) return std::make_pair(false, 0);
  }

  if (bx) {
    while (ax < 4) x[ax++] = 0;
    auto num = Y(dst.data() + j, x, bx);
    j += num;
  }

  return std::make_pair(true, j);
}

std::pair<bool, std::string> decode(Base64 b64, base::Chars src) {
  std::vector<uint8_t> tmp;
  tmp.resize(decoded_length(b64, src.size()));
  auto pair = decode_to(b64, tmp, src);
  if (pair.first) {
    auto* ptr = reinterpret_cast<char*>(tmp.data());
    auto len = pair.second;
    return std::make_pair(true, std::string(ptr, len));
  } else {
    return std::make_pair(false, std::string());
  }
}

}  // namespace encoding
