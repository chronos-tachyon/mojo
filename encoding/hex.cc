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

std::size_t encode_to(Hex hex, base::MutableChars dst, base::Bytes src) noexcept {
  CHECK_GE(dst.size(), encoded_length(hex, src.size()));
  const char* cs = (hex.uppercase ? HEX_UC_CHARSET : HEX_LC_CHARSET);
  std::size_t i, j;
  for (i = 0, j = 0; i < src.size(); ++i) {
    uint8_t byte = src[i];
    dst[j++] = cs[byte >> 4];
    dst[j++] = cs[byte & 15];
  }
  return j;
}

std::string encode(Hex hex, base::Bytes src) {
  std::vector<char> tmp;
  tmp.resize(encoded_length(hex, src.size()));
  auto len = encode_to(hex, tmp, src);
  return std::string(tmp.data(), len);
}

std::size_t decoded_length(Hex hex, std::size_t len) noexcept {
  return (len + 1) / 2;
}

std::pair<bool, std::size_t> decode_to(Hex hex, base::MutableBytes dst, base::Chars src) noexcept {
  CHECK_GE(dst.size(), decoded_length(hex, src.size()));
  std::size_t i, j;
  uint8_t x[2];
  uint8_t nx = 0;
  for (i = 0, j = 0; i < src.size(); ++i) {
    char ch = src[i];
    auto val = from_hex(ch);
    if (val < 16) {
      x[nx++] = val;
      if (nx == 2) {
        dst[j++] = (x[0] << 4) | x[1];
        nx = 0;
      }
    } else if (!is_space(ch)) {
      return std::make_pair(false, 0);
    }
  }
  if (nx) {
    dst[j++] = (x[0] << 4);
  }
  return std::make_pair(true, j);
}

std::pair<bool, std::string> decode(Hex hex, base::Chars src) {
  std::vector<uint8_t> tmp;
  tmp.resize(decoded_length(hex, src.size()));
  auto pair = decode_to(hex, tmp, src);
  if (pair.first) {
    auto* ptr = reinterpret_cast<char*>(tmp.data());
    auto len = pair.second;
    return std::make_pair(true, std::string(ptr, len));
  } else {
    return std::make_pair(false, std::string());
  }
}

}  // namespace encoding
