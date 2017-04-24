// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_HASH_SHA2_H
#define CRYPTO_HASH_SHA2_H

#include "crypto/crypto.h"

namespace crypto {
namespace hash {

constexpr std::size_t SHA224_BLOCKSIZE = 64;
constexpr std::size_t SHA224_SUMSIZE = 28;

constexpr std::size_t SHA256_BLOCKSIZE = 64;
constexpr std::size_t SHA256_SUMSIZE = 32;

constexpr std::size_t SHA512_224_BLOCKSIZE = 128;
constexpr std::size_t SHA512_224_SUMSIZE = 28;

constexpr std::size_t SHA512_256_BLOCKSIZE = 128;
constexpr std::size_t SHA512_256_SUMSIZE = 32;

constexpr std::size_t SHA384_BLOCKSIZE = 128;
constexpr std::size_t SHA384_SUMSIZE = 48;

constexpr std::size_t SHA512_BLOCKSIZE = 128;
constexpr std::size_t SHA512_SUMSIZE = 64;

std::unique_ptr<Hasher> new_sha224();
std::unique_ptr<Hasher> new_sha256();
std::unique_ptr<Hasher> new_sha384();
std::unique_ptr<Hasher> new_sha512();
std::unique_ptr<Hasher> new_sha512_224();
std::unique_ptr<Hasher> new_sha512_256();

}  // namespace hash
}  // namespace crypto

#endif  // CRYPTO_HASH_SHA2_H
