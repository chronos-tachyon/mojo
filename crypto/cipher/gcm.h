// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_CIPHER_GCM_H
#define CRYPTO_CIPHER_GCM_H

#include <memory>

#include "base/bytes.h"
#include "crypto/crypto.h"

namespace crypto {
namespace cipher {

constexpr uint16_t GCM_BLOCKSIZE = 16;
constexpr uint16_t GCM_TAGSIZE = 16;
constexpr uint16_t GCM_NONCESIZE = 12;

std::unique_ptr<Sealer> new_gcm(std::unique_ptr<BlockCrypter> block);

}  // namespace cipher
}  // namespace crypto

#endif  // CRYPTO_CIPHER_GCM_H
