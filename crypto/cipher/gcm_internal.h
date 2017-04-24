// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_CIPHER_GCM_INTERNAL_H
#define CRYPTO_CIPHER_GCM_INTERNAL_H

#include <climits>
#include <cstring>
#include <utility>

#include "base/bytes.h"
#include "base/logging.h"
#include "base/result.h"
#include "crypto/primitives.h"
#include "crypto/subtle.h"

namespace crypto {
namespace cipher {
inline namespace implementation {

static unsigned int GCM_REVERSE(unsigned int value) {
  auto a = (value >> 3) & 1;
  auto b = (value >> 1) & 2;
  auto c = (value << 1) & 4;
  auto d = (value << 3) & 8;
  return a | b | c | d;
}

struct GCMElement {
  uint64_t lo;
  uint64_t hi;

  explicit GCMElement(uint64_t lo, uint64_t hi) noexcept : lo(lo), hi(hi) {}
  GCMElement() noexcept : lo(0), hi(0) {}
  GCMElement(const GCMElement&) noexcept = default;
  GCMElement(GCMElement&&) noexcept = default;
  GCMElement& operator=(const GCMElement&) noexcept = default;
  GCMElement& operator=(GCMElement&&) noexcept = default;

  GCMElement operator^(const GCMElement& b) const noexcept {
    return GCMElement(lo ^ b.lo, hi ^ b.hi);
  }

  GCMElement& operator^=(const GCMElement& b) noexcept {
    lo ^= b.lo;
    hi ^= b.hi;
    return *this;
  }
};

static GCMElement gcm_from_block(const uint8_t* ptr) noexcept {
  return GCMElement(crypto::primitives::RBE64(ptr, 0),
                    crypto::primitives::RBE64(ptr, 1));
}

static void gcm_to_block(uint8_t* ptr, GCMElement value) noexcept {
  crypto::primitives::WBE64(ptr, 0, value.lo);
  crypto::primitives::WBE64(ptr, 1, value.hi);
}

static GCMElement gcm_double(GCMElement value) noexcept {
  GCMElement result;
  bool msb = (value.hi & 1);
  result.lo = (value.lo >> 1);
  result.hi = (value.hi >> 1) | (value.lo << 63);
  if (msb) result.lo ^= 0xe100000000000000ULL;
  return result;
}

static constexpr uint16_t GCM_REDUCTIONS[16] = {
    0x0000, 0x1c20, 0x3840, 0x2460, 0x7080, 0x6ca0, 0x48c0, 0x54e0,
    0xe100, 0xfd20, 0xd940, 0xc560, 0x9180, 0x8da0, 0xa9c0, 0xb5e0,
};

template <typename Functor>
struct GCMKey {
  Functor f;
  GCMElement* product_table;

  GCMKey(Functor fn, GCMElement* ptbl) : f(std::move(fn)), product_table(ptbl) {
    uint8_t h[16];
    ::bzero(h, 16);
    base::MutableBytes hh(h, sizeof(h));
    f(hh, hh);

    auto one = gcm_from_block(h);
    product_table[0] = GCMElement();
    product_table[GCM_REVERSE(1)] = one;
    for (unsigned int i = 2; i < 16; i += 2) {
      auto& x = product_table[GCM_REVERSE(i / 2)];
      auto& y = product_table[GCM_REVERSE(i)];
      auto& z = product_table[GCM_REVERSE(i + 1)];
      y = gcm_double(x);
      z = y ^ one;
    }
  }

  void multiply(GCMElement& x) const noexcept {
    GCMElement tmp;

    auto f = [this, &tmp](uint64_t word) {
      for (unsigned int i = 0; i < 16; ++i) {
        // tmp *= 16
        auto msw = tmp.hi & 0xf;
        tmp.hi = (tmp.hi >> 4) | (tmp.lo << 60);
        tmp.lo = (tmp.lo >> 4) ^ (uint64_t(GCM_REDUCTIONS[msw]) << 48);

        // tmp += word[0] * H
        tmp ^= product_table[word & 0xf];

        // word = word[1:]
        word >>= 4;
      }
    };

    f(x.hi);
    f(x.lo);
    x = tmp;
  }

  void block_update(GCMElement& x, const uint8_t* ptr) const noexcept {
    x ^= gcm_from_block(ptr);
    multiply(x);
  }
};

template <typename Functor>
struct GCMState {
  uint8_t counter[16];
  uint8_t tagmask[16];
  uint8_t partial[16];
  uint8_t keystream[16];
  GCMElement xi;
  const GCMKey<Functor>* key;
  uint64_t additional_len;
  uint64_t ciphertext_len;
  uint8_t saved;
  uint8_t available;

  GCMState(const GCMKey<Functor>* k, base::Bytes nonce) noexcept
      : key(k),
        additional_len(0),
        ciphertext_len(0),
        saved(0),
        available(0) {
    if (nonce.size() == 12) {
      ::memcpy(counter, nonce.data(), 12);
      crypto::primitives::WBE32(counter + 12, 0, 1);
    } else {
      GCMElement n;
      const uint8_t* ptr = nonce.data();
      std::size_t len = nonce.size();
      while (len >= 16) {
        key->block_update(n, ptr);
        ptr += 16;
        len -= 16;
      }
      if (len) {
        ::bzero(partial, 16);
        ::memcpy(partial, ptr, len);
        key->block_update(n, partial);
      }
      n.hi ^= uint64_t(nonce.size()) * 8;
      key->multiply(n);
      gcm_to_block(counter, n);
    }
    key->f(base::MutableBytes(tagmask, 16), base::Bytes(counter, 16));
    incr();
  }

  void incr() noexcept {
    auto ctr = crypto::primitives::RBE32(counter + 12, 0);
    crypto::primitives::WBE32(counter + 12, 0, ctr + 1);
  }

  void next() noexcept {
    DCHECK_EQ(available, 0U);
    key->f(base::MutableBytes(keystream, 16), base::Bytes(counter, 16));
    incr();
    available = 16;
  }

  void update(const uint8_t* ptr, std::size_t len) noexcept {
    if (saved) {
      uint8_t n = 16 - saved;
      if (n > len) n = len;
      ::memcpy(partial + saved, ptr, n);
      saved += n;
      ptr += n;
      len -= n;
      if (saved == 16) {
        key->block_update(xi, partial);
        saved = 0;
      }
    }

    while (len >= 16) {
      key->block_update(xi, ptr);
      ptr += 16;
      len -= 16;
    }

    if (len) {
      ::memcpy(partial, ptr, len);
      saved = len;
    }
  }

  void authenticate(base::Bytes bytes) noexcept {
    update(bytes.data(), bytes.size());
    additional_len += bytes.size() * 8;
  }

  void encrypt(base::MutableBytes dst, base::Bytes src) noexcept {
    DCHECK_GE(dst.size(), src.size());

    uint8_t* dptr = dst.data();
    const uint8_t* sptr = src.data();
    std::size_t len = src.size();
    while (len) {
      if (available == 0) next();
      uint8_t n = available;
      if (n > len) n = len;
      crypto::primitives::memxor(dptr, sptr, keystream, n);
      update(dptr, n);
      dptr += n;
      sptr += n;
      len -= n;
      available -= n;
    }
    ciphertext_len += src.size() * 8;
  }

  void decrypt(base::MutableBytes dst, base::Bytes src) noexcept {
    DCHECK_GE(dst.size(), src.size());

    uint8_t* dptr = dst.data();
    const uint8_t* sptr = src.data();
    std::size_t len = src.size();
    while (len) {
      if (available == 0) next();
      uint8_t n = available;
      if (n > len) n = len;
      update(sptr, n);
      crypto::primitives::memxor(dptr, sptr, keystream, n);
      dptr += n;
      sptr += n;
      len -= n;
      available -= n;
    }
    ciphertext_len += src.size() * 8;
  }

  void flush() noexcept {
    if (saved) {
      ::bzero(partial + saved, 16 - saved);
      key->block_update(xi, partial);
      saved = 0;
    }
  }

  void finish(uint8_t* out) noexcept {
    xi.lo ^= additional_len;
    xi.hi ^= ciphertext_len;
    key->multiply(xi);
    gcm_to_block(out, xi);
    crypto::primitives::memxor(out, out, tagmask, 16);
  }

  void seal(uint8_t* out, base::MutableBytes dst, base::Bytes src,
            base::Bytes additional) noexcept {
    authenticate(additional);
    flush();
    encrypt(dst, src);
    flush();
    finish(out);
  }

  bool unseal(const uint8_t* tag, base::MutableBytes dst, base::Bytes src,
              base::Bytes additional) noexcept {
    uint8_t computed[16];

    authenticate(additional);
    flush();
    decrypt(dst, src);
    flush();
    finish(computed);
    return crypto::subtle::consttime_eq(computed, tag, 16);
  }
};

}  // inline namespace implementation
}  // namespace cipher
}  // namespace crypto

#endif  // CRYPTO_CIPHER_GCM_INTERNAL_H
