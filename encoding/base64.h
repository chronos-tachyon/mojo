// encoding/base64.h - Encode/Decode helpers for base-64 data
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef ENCODING_BASE64_H
#define ENCODING_BASE64_H

#include <string>
#include <utility>

#include "base/strings.h"

namespace encoding {

struct Base64 {
  const char* charset;
  bool pad;

  constexpr Base64(const char* cs, bool p) noexcept : charset(cs), pad(p) {}
};

constexpr char B64_STANDARD_CHARSET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

constexpr char B64_URLSAFE_CHARSET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=";

// Encoder modes {{{

constexpr Base64 BASE64 = {B64_STANDARD_CHARSET, true};
constexpr Base64 BASE64_NOPAD = {B64_STANDARD_CHARSET, false};
constexpr Base64 BASE64_URLSAFE = {B64_URLSAFE_CHARSET, true};
constexpr Base64 BASE64_URLSAFE_NOPAD = {B64_URLSAFE_CHARSET, false};

// }}}

// Returns the buffer size needed to encode a |len|-byte input as base-64.
std::size_t encoded_length(Base64 b64, std::size_t len) noexcept;

// Reads the |len| bytes at |src|, encodes them as base-64, and writes the
// resulting characters to the buffer at |dst|, which must contain space for at
// least |encoded_length(b64, len)| characters.
//
// Returns the actual number of characters that were written to |dst|.
//
std::size_t encode_to(Base64 b64, char* dst, const uint8_t* src,
                      std::size_t len) noexcept;

// Convenience function.
inline std::size_t encode_to(Base64 b64, char* dst, const char* src,
                             std::size_t len) noexcept {
  return encode_to(b64, dst, reinterpret_cast<const uint8_t*>(src), len);
}

// Convenience function.
inline std::size_t encode_to(Base64 b64, char* dst,
                             base::StringPiece src) noexcept {
  return encode_to(b64, dst, src.data(), src.size());
}

// Reads the |len| bytes at |src|, encodes them as base-64, and returns the
// resulting characters as a std::string.
std::string encode(Base64 b64, const uint8_t* src, std::size_t len);

// Convenience function.
inline std::string encode(Base64 b64, const char* src, std::size_t len) {
  return encode(b64, reinterpret_cast<const uint8_t*>(src), len);
}

// Convenience function.
inline std::string encode(Base64 b64, base::StringPiece src) {
  return encode(b64, src.data(), src.size());
}

// Returns the buffer size needed to decode a |len|-char base-64 input.
std::size_t decoded_length(Base64 b64, std::size_t len) noexcept;

// Reads the |len| characters at |src|, decodes them as base-64, and writes the
// resulting bytes to |dst|, which must contain space for at least
// |decoded_length(b64, len)| bytes.
//
// On success, returns |{true, outlen}| where |outlen| is the actual number of
// bytes written to |dst|.
//
// On failure, returns |{false, unspecified}|.
//
std::pair<bool, std::size_t> decode_to(Base64 b64, uint8_t* dst,
                                       const char* src,
                                       std::size_t len) noexcept;

// Convenience function.
inline std::pair<bool, std::size_t> decode_to(Base64 b64, char* dst,
                                              const char* src,
                                              std::size_t len) noexcept {
  return decode_to(b64, reinterpret_cast<uint8_t*>(dst), src, len);
}

// Convenience function.
inline std::pair<bool, std::size_t> decode_to(Base64 b64, uint8_t* dst,
                                              base::StringPiece src) noexcept {
  return decode_to(b64, dst, src.data(), src.size());
}

// Convenience function.
inline std::pair<bool, std::size_t> decode_to(Base64 b64, char* dst,
                                              base::StringPiece src) noexcept {
  return decode_to(b64, dst, src.data(), src.size());
}

// Reads the |len| characters at |src|, decodes them as base-64, and returns
// the resulting bytes as a std::string.
std::pair<bool, std::string> decode(Base64 b64, const char* src,
                                    std::size_t len);

// Convenience function.
inline std::pair<bool, std::string> decode(Base64 b64, base::StringPiece src) {
  return decode(b64, src.data(), src.size());
}

}  // namespace encoding

#endif  // ENCODING_BASE64_H
