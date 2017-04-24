// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/internal.h"

namespace crypto {
namespace internal {

std::string canonical_name(base::StringPiece in) {
  std::string out;
  out.reserve(in.size());
  for (char ch : in) {
    if (ch >= '0' && ch <= '9') {
      out.push_back(ch);
    } else if (ch >= 'a' && ch <= 'z') {
      out.push_back(ch);
    } else if (ch >= 'A' && ch <= 'Z') {
      out.push_back(ch + ('a' - 'A'));
    }
  }
  return out;
}

}  // namespace internal
}  // namespace crypto
