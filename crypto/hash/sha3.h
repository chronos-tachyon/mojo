// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_HASH_SHA3_H
#define CRYPTO_HASH_SHA3_H

#include "crypto/crypto.h"

namespace crypto {
namespace hash {

constexpr std::size_t SHA3_224_BLOCKSIZE = 144;
constexpr std::size_t SHA3_224_SUMSIZE = 28;

constexpr std::size_t SHA3_256_BLOCKSIZE = 136;
constexpr std::size_t SHA3_256_SUMSIZE = 32;

constexpr std::size_t SHA3_384_BLOCKSIZE = 104;
constexpr std::size_t SHA3_384_SUMSIZE = 48;

constexpr std::size_t SHA3_512_BLOCKSIZE = 72;
constexpr std::size_t SHA3_512_SUMSIZE = 64;

constexpr std::size_t SHAKE128_BLOCKSIZE = 168;
constexpr std::size_t SHAKE128_SUGGESTED_SUMSIZE = 32;

constexpr std::size_t SHAKE256_BLOCKSIZE = 136;
constexpr std::size_t SHAKE256_SUGGESTED_SUMSIZE = 64;

std::unique_ptr<Hasher> new_sha3_224();
std::unique_ptr<Hasher> new_sha3_256();
std::unique_ptr<Hasher> new_sha3_384();
std::unique_ptr<Hasher> new_sha3_512();
std::unique_ptr<Hasher> new_shake128(uint16_t d = 0);
std::unique_ptr<Hasher> new_shake256(uint16_t d = 0);

}  // namespace hash
}  // namespace crypto

#endif  // CRYPTO_HASH_SHA3_H
