// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_HASH_MD5_H
#define CRYPTO_HASH_MD5_H

#include "crypto/crypto.h"

namespace crypto {
namespace hash {

constexpr std::size_t MD5_BLOCKSIZE = 64;
constexpr std::size_t MD5_SUMSIZE = 16;

std::unique_ptr<Hasher> new_md5();

}  // namespace hash
}  // namespace crypto

#endif  // CRYPTO_HASH_MD5_H
