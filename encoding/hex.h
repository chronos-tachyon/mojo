// encoding/hex.h - Encode/Decode helpers for base-16 data
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef ENCODING_HEX_H
#define ENCODING_HEX_H

#include <string>
#include <utility>

#include "base/strings.h"

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

// Returns the buffer size needed to encode a |len|-byte input as hexadecimal.
std::size_t encoded_length(Hex hex, std::size_t len) noexcept;

// Reads the |len| bytes at |src|, encodes them as hexadecimal, and writes the
// resulting characters to the buffer at |dst|, which must contain space for at
// least |encoded_length(hex, len)| characters.
//
// Returns the actual number of characters that were written to |dst|.
//
std::size_t encode_to(Hex hex, char* dst, const uint8_t* src,
                      std::size_t len) noexcept;

// Convenience function.
inline std::size_t encode_to(Hex hex, char* dst, const char* src,
                             std::size_t len) noexcept {
  return encode_to(hex, dst, reinterpret_cast<const uint8_t*>(src), len);
}

// Convenience function.
inline std::size_t encode_to(Hex hex, char* dst,
                             base::StringPiece src) noexcept {
  return encode_to(hex, dst, src.data(), src.size());
}

// Reads the |len| bytes at |src|, encodes them as hexadecimal, and returns the
// resulting characters as a std::string.
std::string encode(Hex hex, const uint8_t* src, std::size_t len);

// Convenience function.
inline std::string encode(Hex hex, const char* src, std::size_t len) {
  return encode(hex, reinterpret_cast<const uint8_t*>(src), len);
}

// Convenience function.
inline std::string encode(Hex hex, base::StringPiece src) {
  return encode(hex, src.data(), src.size());
}

// Returns the buffer size needed to decode a |len|-char hexadecimal input.
std::size_t decoded_length(Hex hex, std::size_t len) noexcept;

// Reads the |len| characters at |src|, decodes them as hexadecimal, and writes
// the resulting bytes to |dst|, which must contain space for at least
// |decoded_length(hex, len)| bytes.
//
// On success, returns |{true, outlen}| where |outlen| is the actual number of
// bytes written to |dst|.
//
// On failure, returns |{false, unspecified}|.
//
std::pair<bool, std::size_t> decode_to(Hex hex, uint8_t* dst, const char* src,
                                       std::size_t len) noexcept;

// Convenience function.
inline std::pair<bool, std::size_t> decode_to(Hex hex, char* dst,
                                              const char* src,
                                              std::size_t len) noexcept {
  return decode_to(hex, reinterpret_cast<uint8_t*>(dst), src, len);
}

// Convenience function.
inline std::pair<bool, std::size_t> decode_to(Hex hex, uint8_t* dst,
                                              base::StringPiece src) noexcept {
  return decode_to(hex, dst, src.data(), src.size());
}

// Convenience function.
inline std::pair<bool, std::size_t> decode_to(Hex hex, char* dst,
                                              base::StringPiece src) noexcept {
  return decode_to(hex, dst, src.data(), src.size());
}

// Reads the |len| characters at |src|, decodes them as hexadecimal, and
// returns the resulting bytes as a std::string.
std::pair<bool, std::string> decode(Hex hex, const char* src, std::size_t len);

// Convenience function.
inline std::pair<bool, std::string> decode(Hex hex, base::StringPiece src) {
  return decode(hex, src.data(), src.size());
}

}  // namespace encoding

#endif  // ENCODING_HEX_H
