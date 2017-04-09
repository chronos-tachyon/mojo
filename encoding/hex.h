// encoding/hex.h - Encode/Decode helpers for base-16 data
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef ENCODING_HEX_H
#define ENCODING_HEX_H

#include <string>
#include <utility>

#include "base/bytes.h"
#include "base/chars.h"

namespace encoding {

struct Hex {
  bool uppercase;

  constexpr Hex(bool uc) noexcept : uppercase(uc) {}
};

constexpr char HEX_LC_CHARSET[] = "0123456789abcdef";
constexpr char HEX_UC_CHARSET[] = "0123456789ABCDEF";

// Encoder modes {{{

constexpr Hex HEX = {false};
constexpr Hex HEX_UPPERCASE = {true};

// }}}

// Returns the buffer size needed to encode a |len|-byte input as base-16.
std::size_t encoded_length(Hex hex, std::size_t len) noexcept;

// Reads the bytes in |src|, encodes them as base-16, and writes the resulting
// characters to |dst|, which must contain space for at least
// |encoded_length(hex, len)| characters.
//
// Returns the actual number of characters that were written to |dst|.
//
std::size_t encode_to(Hex hex, base::MutableChars dst,
                      base::Bytes src) noexcept;

// Reads the bytes in |src|, encodes them as base-16, and returns the resulting
// characters as a std::string.
std::string encode(Hex hex, base::Bytes src);

// Returns the buffer size needed to decode a |len|-char base-16 input.
std::size_t decoded_length(Hex hex, std::size_t len) noexcept;

// Reads the characters in |src|, decodes them as base-16, and writes the
// resulting bytes to |dst|, which must contain space for at least
// |decoded_length(hex, len)| bytes.
//
// On success, returns |{true, outlen}| where |outlen| is the actual number of
// bytes written to |dst|.
//
// On failure, returns |{false, unspecified}|.
//
std::pair<bool, std::size_t> decode_to(Hex hex, base::MutableBytes dst,
                                       base::Chars src) noexcept;

// Reads the characters in |src|, decodes them as base-16, and returns the
// resulting bytes as a std::string.
std::pair<bool, std::string> decode(Hex hex, base::Chars src);

// Convenience functions.
inline std::size_t encode_to(Hex hex, base::MutableChars dst,
                             base::Chars src) noexcept {
  base::Bytes bsrc = src;
  return encode_to(hex, dst, bsrc);
}
inline std::string encode(Hex hex, base::Chars src) {
  base::Bytes bsrc = src;
  return encode(hex, bsrc);
}
inline std::pair<bool, std::size_t> decode_to(Hex hex, base::MutableChars dst,
                                              base::Chars src) noexcept {
  return decode_to(hex, base::MutableBytes(dst), src);
}

}  // namespace encoding

#endif  // ENCODING_HEX_H
