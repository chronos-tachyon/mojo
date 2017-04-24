// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_CIPHER_CBC_INTERNAL_H
#define CRYPTO_CIPHER_CBC_INTERNAL_H

#include <cstring>
#include <utility>

#include "base/bytes.h"
#include "base/logging.h"
#include "crypto/primitives.h"

template <typename Functor>
static inline void cbc_encrypt(base::MutableBytes iv,
                               base::MutableBytes dst,
                               base::Bytes src, Functor f) {
  CHECK_GE(dst.size(), src.size());

  uint8_t* dptr = dst.data();
  const uint8_t* sptr = src.data();
  std::size_t len = src.size();
  std::size_t blksz = iv.size();
  while (len >= blksz) {
    crypto::primitives::memxor(dptr, sptr, iv.data(), blksz);
    f(base::MutableBytes(dptr, blksz), base::Bytes(dptr, blksz));
    ::memcpy(iv.data(), dptr, blksz);
    dptr += blksz;
    sptr += blksz;
    len -= blksz;
  }
  CHECK_EQ(len, 0U);
}

template <typename Functor>
static inline void cbc_decrypt(base::MutableBytes iv,
                               base::MutableBytes scratch,
                               base::MutableBytes dst,
                               base::Bytes src, Functor f) {
  CHECK_GE(dst.size(), src.size());

  uint8_t* dptr = dst.data();
  const uint8_t* sptr = src.data();
  std::size_t len = src.size();
  std::size_t blksz = iv.size();
  while (len >= blksz) {
    ::memcpy(scratch.data(), sptr, blksz);
    f(base::MutableBytes(dptr, blksz), base::Bytes(sptr, blksz));
    crypto::primitives::memxor(dptr, dptr, iv.data(), blksz);
    ::memcpy(iv.data(), scratch.data(), blksz);
    dptr += blksz;
    sptr += blksz;
    len -= blksz;
  }
  CHECK_EQ(len, 0U);
}

#endif  // CRYPTO_CIPHER_CBC_INTERNAL_H
