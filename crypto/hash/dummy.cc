// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/hash.h"

namespace crypto {
namespace hash {

const Algorithm MD4 = {
    ID::md4, "MD4", 0, 0, Security::broken, nullptr, nullptr,
};

const Algorithm RIPEMD160 = {
    ID::ripemd160, "RIPEMD160", 0, 0, Security::weak, nullptr, nullptr,
};

}  // namespace hash
}  // namespace crypto
