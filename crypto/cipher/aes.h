// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_CIPHER_AES_H
#define CRYPTO_CIPHER_AES_H

#include <memory>

#include "base/bytes.h"
#include "crypto/crypto.h"

namespace crypto {
namespace cipher {

constexpr uint32_t AES_BLOCKSIZE = 16;
constexpr uint32_t AES128_KEYSIZE = 16;
constexpr uint32_t AES192_KEYSIZE = 24;
constexpr uint32_t AES256_KEYSIZE = 32;

constexpr uint32_t AES_GCM_NONCESIZE = 12;
constexpr uint32_t AES_GCM_TAGSIZE = 16;

std::unique_ptr<BlockCrypter> new_aes(base::Bytes key);
std::unique_ptr<Crypter> new_aes_cbc(base::Bytes key, base::Bytes iv);
std::unique_ptr<Crypter> new_aes_ctr(base::Bytes key, base::Bytes iv);
std::unique_ptr<Sealer> new_aes_gcm(base::Bytes key);

}  // namespace cipher
}  // namespace crypto

#endif  // CRYPTO_CIPHER_AES_H
