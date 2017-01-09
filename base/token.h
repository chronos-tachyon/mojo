// base/token.h - Value type representing a unique opaque token
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_TOKEN_H
#define BASE_TOKEN_H

#include <cstdint>
#include <functional>

namespace base {

// token_t represents a mostly-opaque token.  The primary function is to
// provide distinct values which are comparable for (in)equality, but the class
// is also usable as a map key.
class token_t {
 public:
  constexpr explicit token_t(uint64_t value) noexcept : value_(value) {}

  constexpr token_t() noexcept : value_(0) {}
  constexpr token_t(const token_t&) noexcept = default;
  constexpr token_t(token_t&&) noexcept = default;
  token_t& operator=(const token_t&) noexcept = default;
  token_t& operator=(token_t&&) noexcept = default;

  constexpr explicit operator uint64_t() const noexcept { return value_; }

 private:
  uint64_t value_;
};

inline constexpr bool operator==(token_t a, token_t b) noexcept {
  return uint64_t(a) == uint64_t(b);
}

inline constexpr bool operator!=(token_t a, token_t b) noexcept {
  return !(a == b);
}

inline constexpr bool operator<(token_t a, token_t b) noexcept {
  return uint64_t(a) < uint64_t(b);
}

inline constexpr bool operator>(token_t a, token_t b) noexcept {
  return (b < a);
}

inline constexpr bool operator<=(token_t a, token_t b) noexcept {
  return !(b < a);
}

inline constexpr bool operator>=(token_t a, token_t b) noexcept {
  return !(a < b);
}

// Returns a new token, unique within this process.
//
// This function never returns the "null" (default-constructed) token.
token_t next_token() noexcept;

}  // namespace base

namespace std {
template <>
struct hash<base::token_t> {
  std::size_t operator()(base::token_t t) const noexcept { return uint64_t(t); }
};
}  // namespace std

#endif  // BASE_TOKEN_H
