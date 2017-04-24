// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_HASH_SHA1_H
#define CRYPTO_HASH_SHA1_H

#include "crypto/crypto.h"

namespace crypto {
namespace hash {

constexpr std::size_t SHA1_BLOCKSIZE = 64;
constexpr std::size_t SHA1_SUMSIZE = 20;

std::unique_ptr<Hasher> new_sha1();

}  // namespace hash
}  // namespace crypto

#endif  // CRYPTO_HASH_SHA1_H
