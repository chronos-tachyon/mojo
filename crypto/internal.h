// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_INTERNAL_H
#define CRYPTO_INTERNAL_H

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/result.h"
#include "base/strings.h"
#include "crypto/security.h"

namespace crypto {

struct Hash;             // forward declaration
struct BlockCipher;      // forward declaration
struct BlockCipherMode;  // forward declaration
struct StreamCipher;     // forward declaration

namespace internal {

extern std::mutex g_mu;
extern std::vector<const Hash*>* g_hash;
extern std::vector<const BlockCipher*>* g_block;
extern std::vector<const BlockCipherMode*>* g_mode;
extern std::vector<const StreamCipher*>* g_stream;

}  // namespace internal
}  // namespace crypto

#endif  // CRYPTO_INTERNAL_H
