// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_CIPHER_CHACHA20_H
#define CRYPTO_CIPHER_CHACHA20_H

#include <memory>

#include "base/bytes.h"
#include "crypto/crypto.h"

namespace crypto {
namespace cipher {

constexpr uint32_t CHACHA20_BLOCKSIZE = 64;
constexpr uint32_t CHACHA20_KEYSIZE_HALF = 16;
constexpr uint32_t CHACHA20_KEYSIZE_FULL = 32;
constexpr uint32_t CHACHA20_NONCESIZE = 12;

std::unique_ptr<Crypter> new_chacha20(base::Bytes key, base::Bytes nonce);

}  // namespace cipher
}  // namespace crypto

#endif  // CRYPTO_CIPHER_CHACHA20_H
