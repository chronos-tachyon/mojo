// crypto/hash/keccak.h - Utility functions for hashes built on Keccak
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_HASH_KECCAK_H
#define CRYPTO_HASH_KECCAK_H

#include <cstdint>

namespace crypto {
namespace hash {

void keccak_f1600_xor_in(uint64_t* state, const uint8_t* in, unsigned int len);
void keccak_f1600_copy_out(uint8_t* out, unsigned int len,
                           const uint64_t* state);
void keccak_f1600_permute(uint64_t* state);

}  // namespace hash
}  // namespace crypto

#endif  // CRYPTO_HASH_KECCAK_H
