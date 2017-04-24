// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/security.h"

#include <ostream>

namespace crypto {

static constexpr base::StringPiece SECURITY_BROKEN = "broken";
static constexpr base::StringPiece SECURITY_WEAK = "weak";
static constexpr base::StringPiece SECURITY_SECURE = "secure";
static constexpr base::StringPiece SECURITY_STRONG = "strong";

base::StringPiece security_name(Security sec) {
  if (sec < Security::weak) {
    return SECURITY_BROKEN;
  } else if (sec < Security::secure) {
    return SECURITY_WEAK;
  } else if (sec < Security::strong) {
    return SECURITY_SECURE;
  } else {
    return SECURITY_STRONG;
  }
}

void append_to(std::string* out, Security sec) {
  security_name(sec).append_to(out);
}

std::size_t length_hint(Security sec) { return 6; }

std::ostream& operator<<(std::ostream& o, Security sec) {
  std::string str;
  append_to(&str, sec);
  return (o << str);
}

}  // namespace crypto
