// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_CIPHER_DES_H
#define CRYPTO_CIPHER_DES_H

#include <memory>

#include "base/bytes.h"
#include "crypto/crypto.h"

namespace crypto {
namespace cipher {

constexpr uint32_t DES_BLOCKSIZE = 8;
constexpr uint32_t DES_KEYSIZE = 8;

constexpr uint32_t TRIPLEDES_BLOCKSIZE = 8;
constexpr uint32_t TRIPLEDES_KEYSIZE = 24;

std::unique_ptr<BlockCrypter> new_des(base::Bytes key);
std::unique_ptr<BlockCrypter> new_3des(base::Bytes key);

}  // namespace cipher
}  // namespace crypto

#endif  // CRYPTO_CIPHER_DES_H
