// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/des.h"

#include "crypto/cipher/_des.h"
#include "crypto/primitives.h"

using crypto::primitives::ROL32;
using crypto::primitives::RBE64;
using crypto::primitives::WBE64;

static uint64_t P(uint64_t in, const uint8_t* perm, std::size_t n) {
  uint64_t out = 0;
  for (unsigned int i = 0; i < n; ++i) {
    uint64_t bit = (in >> perm[i]) & 1;
    out |= bit << (n - i - 1);
  }
  return out;
}

template <std::size_t N>
static uint64_t P(uint64_t in, const uint8_t (&perm)[N]) {
  return P(in, perm, N);
}

static uint32_t F(uint32_t in, uint64_t subkey) {
  uint64_t locations = subkey ^ P(in, EXPANSION_FUNCTION);
  uint32_t out = 0;
  for (unsigned int s = 0; s < 8; ++s) {
    uint8_t loc = (locations >> 42) & 63;
    locations <<= 6;
    uint8_t i = ((loc >> 4) & 2) | (loc & 1);
    uint8_t j = (loc >> 1) & 15;
    uint64_t f = uint64_t(SBOX[s][i][j]) << (4 * (7 - s));
    f = P(f, PERMUTATION_FUNCTION);
    out ^= f;
  }
  return out;
}

namespace crypto {
namespace cipher {
inline namespace implementation {
struct DESState {
  uint64_t subkeys[16];
};

struct TripleDESState {
  DESState one;
  DESState two;
  DESState three;
};

static void des_expand_key(DESState* state, const uint8_t* key, uint32_t len) {
  CHECK_NOTNULL(key);

  switch (len) {
    case DES_KEYSIZE:
      break;

    default:
      throw std::invalid_argument("key size not supported");
  }

  uint64_t block = RBE64(key, 0);
  block = P(block, PERMUTED_CHOICE_1);

  auto fn = [](uint32_t* out, uint32_t in) {
    uint32_t tmp = in & 0x0fffffffU;
    for (unsigned int i = 0; i < 16; ++i) {
      unsigned int w = KS_ROTATIONS[i];
      tmp <<= 4;
      uint32_t x = (tmp << w) >> 4;
      uint32_t y = (tmp >> (32 - w));
      tmp = (x | y);
      out[i] = tmp;
    }
  };

  uint32_t xx[16];
  uint32_t yy[16];
  fn(xx, block >> 28);
  fn(yy, block);

  for (unsigned int i = 0; i < 16; ++i) {
    block = (uint64_t(xx[i]) << 28) | uint64_t(yy[i]);
    state->subkeys[i] = P(block, PERMUTED_CHOICE_2);
  }
}

static void des_expand_key(DESState* state, base::Bytes key) {
  des_expand_key(state, key.data(), key.size());
}

static void des_crypt(const DESState* state, uint8_t* dst, const uint8_t* src,
                      std::size_t len, bool decrypt) noexcept {
  CHECK_NOTNULL(dst);
  CHECK_NOTNULL(src);

  while (len >= 8) {
    uint64_t block = RBE64(src, 0);
    block = P(block, INITIAL_PERMUTATION);
    uint32_t x = block >> 32;
    uint32_t y = block;
    uint32_t z;

    uint64_t subkey;
    for (unsigned int i = 0; i < 16; ++i) {
      if (decrypt) {
        subkey = state->subkeys[15 - i];
      } else {
        subkey = state->subkeys[i];
      }
      z = x ^ F(y, subkey);
      x = y;
      y = z;
    }

    block = (uint64_t(y) << 32) | uint64_t(x);
    block = P(block, FINAL_PERMUTATION);
    WBE64(dst, 0, block);

    dst += 8;
    src += 8;
    len -= 8;
  }
  DCHECK_EQ(len, 0U);
}

static void des_encrypt(const DESState* state, uint8_t* dst, const uint8_t* src,
                        std::size_t len) noexcept {
  des_crypt(state, dst, src, len, false);
}

static void des_encrypt(const DESState* state, base::MutableBytes dst,
                        base::Bytes src) noexcept {
  DCHECK_GE(dst.size(), src.size());
  des_encrypt(state, dst.data(), src.data(), src.size());
}

static void des_decrypt(const DESState* state, uint8_t* dst, const uint8_t* src,
                        std::size_t len) noexcept {
  des_crypt(state, dst, src, len, true);
}

static void des_decrypt(const DESState* state, base::MutableBytes dst,
                        base::Bytes src) noexcept {
  DCHECK_GE(dst.size(), src.size());
  des_decrypt(state, dst.data(), src.data(), src.size());
}

static void tripledes_expand_key(TripleDESState* state, const uint8_t* key,
                                 uint32_t len) {
  CHECK_NOTNULL(key);

  switch (len) {
    case TRIPLEDES_KEYSIZE:
      break;

    default:
      throw std::invalid_argument("key size not supported");
  }

  des_expand_key(&state->one, key, 8);
  des_expand_key(&state->two, key + 8, 8);
  des_expand_key(&state->three, key + 16, 8);
}

static void tripledes_expand_key(TripleDESState* state, base::Bytes key) {
  tripledes_expand_key(state, key.data(), key.size());
}

static void tripledes_encrypt(const TripleDESState* state, uint8_t* dst,
                              const uint8_t* src, std::size_t len) noexcept {
  while (len >= 8) {
    des_encrypt(&state->one, dst, src, 8);
    des_decrypt(&state->two, dst, dst, 8);
    des_encrypt(&state->three, dst, dst, 8);

    dst += 8;
    src += 8;
    len -= 8;
  }
  DCHECK_EQ(len, 0U);
}

static void tripledes_encrypt(const TripleDESState* state,
                              base::MutableBytes dst,
                              base::Bytes src) noexcept {
  DCHECK_GE(dst.size(), src.size());
  tripledes_encrypt(state, dst.data(), src.data(), src.size());
}

static void tripledes_decrypt(const TripleDESState* state, uint8_t* dst,
                              const uint8_t* src, std::size_t len) noexcept {
  while (len >= 8) {
    des_decrypt(&state->three, dst, src, 8);
    des_encrypt(&state->two, dst, dst, 8);
    des_decrypt(&state->one, dst, dst, 8);

    dst += 8;
    src += 8;
    len -= 8;
  }
  DCHECK_EQ(len, 0U);
}

static void tripledes_decrypt(const TripleDESState* state,
                              base::MutableBytes dst,
                              base::Bytes src) noexcept {
  DCHECK_GE(dst.size(), src.size());
  tripledes_decrypt(state, dst.data(), src.data(), src.size());
}

class DESBlockCrypter : public BlockCrypter {
 public:
  DESBlockCrypter(base::Bytes key);
  uint16_t block_size() const noexcept override { return DES_BLOCKSIZE; }
  void block_encrypt(base::MutableBytes dst, base::Bytes src) const
      noexcept override;
  void block_decrypt(base::MutableBytes dst, base::Bytes src) const
      noexcept override;

 private:
  crypto::subtle::SecureMemory<DESState> state_;
};

DESBlockCrypter::DESBlockCrypter(base::Bytes key) {
  des_expand_key(state_.get(), key);
}

void DESBlockCrypter::block_encrypt(base::MutableBytes dst,
                                    base::Bytes src) const noexcept {
  des_encrypt(state_.get(), dst, src);
}

void DESBlockCrypter::block_decrypt(base::MutableBytes dst,
                                    base::Bytes src) const noexcept {
  CHECK_GE(dst.size(), src.size());
  des_decrypt(state_.get(), dst, src);
}

class TripleDESBlockCrypter : public BlockCrypter {
 public:
  TripleDESBlockCrypter(base::Bytes key);
  uint16_t block_size() const noexcept override { return TRIPLEDES_BLOCKSIZE; }
  void block_encrypt(base::MutableBytes dst, base::Bytes src) const
      noexcept override;
  void block_decrypt(base::MutableBytes dst, base::Bytes src) const
      noexcept override;

 private:
  crypto::subtle::SecureMemory<TripleDESState> state_;
};

TripleDESBlockCrypter::TripleDESBlockCrypter(base::Bytes key) {
  tripledes_expand_key(state_.get(), key);
}

void TripleDESBlockCrypter::block_encrypt(base::MutableBytes dst,
                                          base::Bytes src) const noexcept {
  tripledes_encrypt(state_.get(), dst, src);
}

void TripleDESBlockCrypter::block_decrypt(base::MutableBytes dst,
                                          base::Bytes src) const noexcept {
  tripledes_decrypt(state_.get(), dst, src);
}
}  // inline namespace implementation

std::unique_ptr<BlockCrypter> new_des(base::Bytes key) {
  return base::backport::make_unique<DESBlockCrypter>(key);
}

std::unique_ptr<BlockCrypter> new_3des(base::Bytes key) {
  return base::backport::make_unique<TripleDESBlockCrypter>(key);
}
}  // namespace cipher
}  // namespace crypto

static const crypto::BlockCipher DES = {
    crypto::cipher::DES_BLOCKSIZE,  // block_size
    crypto::cipher::DES_KEYSIZE,    // key_size
    crypto::Security::broken,       // security
    0,                              // flags
    "DES",                          // name
    crypto::cipher::new_des,        // newfn
    nullptr,                        // cbcfn
    nullptr,                        // ctrfn
    nullptr,                        // gcmfn
};

static const crypto::BlockCipher TRIPLEDES = {
    crypto::cipher::TRIPLEDES_BLOCKSIZE,  // block_size
    crypto::cipher::TRIPLEDES_KEYSIZE,    // key_size
    crypto::Security::weak,               // security
    0,                                    // flags
    "3DES",                               // name
    crypto::cipher::new_3des,             // newfn
    nullptr,                              // cbcfn
    nullptr,                              // ctrfn
    nullptr,                              // gcmfn
};

static void init() __attribute__((constructor));
static void init() {
  crypto::register_block_cipher(&DES);
  crypto::register_block_cipher(&TRIPLEDES);
}
