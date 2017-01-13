// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/strings.h"

#include <ostream>

namespace base {

constexpr std::size_t StringPiece::npos;

void StringPiece::append_to(std::string* out) const {
  out->append(data_, size_);
}

std::ostream& operator<<(std::ostream& o, StringPiece sp) {
  return (o << sp.as_string());
}

}  // namespace base
