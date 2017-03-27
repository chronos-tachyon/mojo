// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "encoding/hex.h"

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

static uint8_t from_hex(char ch) {
  if (ch >= '0' && ch <= '9')
    return (ch - '0');
  else if (ch >= 'a' && ch <= 'f')
    return (ch - 'a' + 10);
  else if (ch >= 'A' && ch <= 'F')
    return (ch - 'A' + 10);
  else
    return NPOS;
}

namespace encoding {

std::size_t encoded_length(Hex hex, std::size_t len) noexcept {
  return len * 2;
}

std::size_t encode_to(Hex hex, char* dst, const uint8_t* src,
                      std::size_t len) noexcept {
  const char* cs = (hex.uppercase ? HEX_UC_CHARSET : HEX_LC_CHARSET);
  std::size_t n = 0;

  while (len) {
    uint8_t byte = *src;
    dst[0] = cs[byte >> 4];
    dst[1] = cs[byte & 15];
    dst += 2;
    n += 2;
    ++src;
    --len;
  }

  return n;
}

std::string encode(Hex hex, const uint8_t* src, std::size_t len) {
  std::vector<char> tmp;
  tmp.resize(encoded_length(hex, len));
  auto n = encode_to(hex, tmp.data(), src, len);
  return std::string(tmp.data(), n);
}

std::size_t decoded_length(Hex hex, std::size_t len) noexcept {
  return (len + 1) / 2;
}

std::pair<bool, std::size_t> decode_to(Hex hex, uint8_t* dst, const char* src,
                                       std::size_t len) noexcept {
  std::size_t n = 0;
  uint8_t x[2];
  uint8_t nx = 0;

  while (len) {
    char ch = src[0];
    ++src;
    --len;

    auto val = from_hex(ch);
    if (val < 16) {
      x[nx++] = val;
    } else if (is_space(ch)) {
      continue;
    } else {
      return std::make_pair(false, 0);
    }

    if (nx == 2) {
      *dst = (x[0] << 4) | x[1];
      ++dst;
      ++n;
      nx = 0;
    }
  }

  if (nx) {
    *dst = (x[0] << 4);
    ++n;
  }

  return std::make_pair(true, n);
}

std::pair<bool, std::string> decode(Hex hex, const char* src, std::size_t len) {
  std::vector<uint8_t> tmp;
  tmp.resize(decoded_length(hex, len));
  auto pair = decode_to(hex, tmp.data(), src, len);
  if (pair.first) {
    auto* ptr = reinterpret_cast<char*>(tmp.data());
    auto n = pair.second;
    return std::make_pair(true, std::string(ptr, n));
  } else {
    return std::make_pair(false, std::string());
  }
}

}  // namespace encoding
