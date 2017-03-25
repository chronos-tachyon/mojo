// crypto/common/common.h - Utility functions for other crypto packages
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_COMMON_COMMON_H
#define CRYPTO_COMMON_COMMON_H

#include <iosfwd>
#include <string>

#include "base/strings.h"

namespace crypto {
namespace common {

enum class Security : unsigned char {
  broken = 0x00,
  weak = 0x40,
  secure = 0x80,
  strong = 0xc0,
};

base::StringPiece security_name(Security sec);
void append_to(std::string* out, Security sec);
std::size_t length_hint(Security sec);
std::ostream& operator<<(std::ostream& o, Security sec);

// Makes sausage out of an algorithm name.  Sausages may be compared for
// equality, enabling human-friendly matching of algorithm names.
std::string canonical_name(base::StringPiece in);

}  // namespace common
}  // namespace crypto

#endif  // CRYPTO_COMMON_COMMON_H
