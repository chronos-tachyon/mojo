// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_SECURITY_H
#define CRYPTO_SECURITY_H

#include <iosfwd>
#include <string>

#include "base/strings.h"

namespace crypto {

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

}  // namespace crypto

#endif  // CRYPTO_SECURITY_H
