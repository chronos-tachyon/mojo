// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/chars.h"

#include <ostream>

#include "base/logging.h"

namespace base {
namespace internal {

void append_chars(std::string* out, const char* ptr, std::size_t len) {
  CHECK_NOTNULL(out);
  out->append(ptr, len);
}

}  // namespace internal

template<> constexpr std::size_t BasicChars<false>::npos;
template<> constexpr std::size_t BasicChars<true>::npos;

std::ostream& operator<<(std::ostream& o, Chars chars) {
  return (o << chars.as_string());
}

}  // namespace base
