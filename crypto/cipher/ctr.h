// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_CIPHER_CTR_H
#define CRYPTO_CIPHER_CTR_H

#include <memory>

#include "base/bytes.h"
#include "crypto/crypto.h"

namespace crypto {
namespace cipher {

std::unique_ptr<Crypter> new_ctr(std::unique_ptr<BlockCrypter> block, base::Bytes iv);

}  // namespace cipher
}  // namespace crypto

#endif  // CRYPTO_CIPHER_CTR_H
