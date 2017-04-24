// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_CIPHER_RC4_H
#define CRYPTO_CIPHER_RC4_H

#include <memory>

#include "base/bytes.h"
#include "crypto/crypto.h"

namespace crypto {
namespace cipher {

constexpr uint32_t RC4_BLOCKSIZE = 256;
constexpr uint32_t RC4_KEYSIZE = 256;
constexpr uint32_t RC4_NONCESIZE = 0;

std::unique_ptr<Crypter> new_rc4(base::Bytes key, base::Bytes nonce);

}  // namespace cipher
}  // namespace crypto

#endif  // CRYPTO_CIPHER_RC4_H
