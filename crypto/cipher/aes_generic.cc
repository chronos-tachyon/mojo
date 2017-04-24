// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/aes_internal.h"

#include <cstdint>

// FIXME
#include <iomanip>
#include <iostream>
#include <sstream>

#include "base/logging.h"
#include "crypto/primitives.h"
#include "crypto/security.h"

using crypto::primitives::ROL32;
using crypto::primitives::RBE32;
using crypto::primitives::WBE32;
using crypto::primitives::RBE64;
using crypto::primitives::WBE64;

static inline uint32_t R(uint32_t w) noexcept { return ROL32(w, 8); }

static inline uint32_t S0(uint32_t a, uint32_t b, uint32_t c,
                          uint32_t d) noexcept {
  a = SBOX_0[(a >> 24) & 0xff] << 24;
  b = SBOX_0[(b >> 16) & 0xff] << 16;
  c = SBOX_0[(c >> 8) & 0xff] << 8;
  d = SBOX_0[(d >> 0) & 0xff] << 0;
  return a | b | c | d;
}

static inline uint32_t S0(uint32_t w) noexcept { return S0(w, w, w, w); }

static inline uint32_t S1(uint32_t a, uint32_t b, uint32_t c,
                          uint32_t d) noexcept {
  a = SBOX_1[(a >> 24) & 0xff] << 24;
  b = SBOX_1[(b >> 16) & 0xff] << 16;
  c = SBOX_1[(c >> 8) & 0xff] << 8;
  d = SBOX_1[(d >> 0) & 0xff] << 0;
  return a | b | c | d;
}

static inline uint32_t S1(uint32_t w) noexcept { return S1(w, w, w, w); }

static inline uint32_t TE(uint32_t a, uint32_t b, uint32_t c,
                          uint32_t d) noexcept {
  a = TE_0[(a >> 24) & 0xff];
  b = TE_1[(b >> 16) & 0xff];
  c = TE_2[(c >> 8) & 0xff];
  d = TE_3[(d >> 0) & 0xff];
  return a ^ b ^ c ^ d;
}

static inline uint32_t TD(uint32_t a, uint32_t b, uint32_t c,
                          uint32_t d) noexcept {
  a = TD_0[(a >> 24) & 0xff];
  b = TD_1[(b >> 16) & 0xff];
  c = TD_2[(c >> 8) & 0xff];
  d = TD_3[(d >> 0) & 0xff];
  return a ^ b ^ c ^ d;
}

static inline uint32_t TDS0(uint32_t w) noexcept {
  uint32_t a = TD_0[SBOX_0[(w >> 24) & 0xff]];
  uint32_t b = TD_1[SBOX_0[(w >> 16) & 0xff]];
  uint32_t c = TD_2[SBOX_0[(w >> 8) & 0xff]];
  uint32_t d = TD_3[SBOX_0[(w >> 0) & 0xff]];
  return a ^ b ^ c ^ d;
}

#define W0(x) ((x).u32.w0)
#define W1(x) ((x).u32.w1)
#define W2(x) ((x).u32.w2)
#define W3(x) ((x).u32.w3)

namespace crypto {
namespace cipher {
inline namespace implementation {
void aes_generic_expand_key(AESState* state, const uint8_t* key,
                            std::size_t len) noexcept {
  auto* const enc = state->enc;
  auto* const dec = state->dec;

  state->num_rounds = (len / 4) + 7;

  switch (len) {
    case 16:

#define ROUND(N, RCON)                                                  \
  do {                                                                  \
    W0(enc[N]) = W0(enc[N - 1]) ^ S0(R(W3(enc[N - 1]))) ^ (RCON << 24); \
    W1(enc[N]) = W1(enc[N - 1]) ^ W0(enc[N]);                           \
    W2(enc[N]) = W2(enc[N - 1]) ^ W1(enc[N]);                           \
    W3(enc[N]) = W3(enc[N - 1]) ^ W2(enc[N]);                           \
  } while (0)

      W0(enc[0]) = RBE32(key, 0);
      W1(enc[0]) = RBE32(key, 1);
      W2(enc[0]) = RBE32(key, 2);
      W3(enc[0]) = RBE32(key, 3);

      ROUND(1, 0x01);
      ROUND(2, 0x02);
      ROUND(3, 0x04);
      ROUND(4, 0x08);
      ROUND(5, 0x10);
      ROUND(6, 0x20);
      ROUND(7, 0x40);
      ROUND(8, 0x80);
      ROUND(9, 0x1b);
      ROUND(10, 0x36);

      DCHECK_EQ(11U, state->num_rounds);

#undef ROUND

      break;

    case 24:

// AES-192 has a weird key schedule:
//
//  0: [A] K K K K
//  1: [B] K K 1 1
//  2: [C] 1 1 1 1
//
//  3: [A] 2 2 2 2
//  4: [B] 2 2 3 3
//  5: [C] 3 3 3 3
//
//  6: [A] 4 4 4 4
//  7: [B] 4 4 5 5
//  8: [C] 5 5 5 5
//
//  9: [A] 6 6 6 6
// 10: [B] 6 6 7 7
// 11: [C] 7 7 7 7
//
// 12: [A] 8 8 8 8

#define ROUND_A(N, RCON)                                                \
  do {                                                                  \
    W0(enc[N]) = W2(enc[N - 2]) ^ S0(R(W3(enc[N - 1]))) ^ (RCON << 24); \
    W1(enc[N]) = W3(enc[N - 2]) ^ W0(enc[N]);                           \
    W2(enc[N]) = W0(enc[N - 1]) ^ W1(enc[N]);                           \
    W3(enc[N]) = W1(enc[N - 1]) ^ W2(enc[N]);                           \
  } while (0)
#define ROUND_B(N, RCON)                                            \
  do {                                                              \
    W0(enc[N]) = W2(enc[N - 2]) ^ W3(enc[N - 1]);                   \
    W1(enc[N]) = W3(enc[N - 2]) ^ W0(enc[N]);                       \
    W2(enc[N]) = W0(enc[N - 1]) ^ S0(R(W1(enc[N]))) ^ (RCON << 24); \
    W3(enc[N]) = W1(enc[N - 1]) ^ W2(enc[N]);                       \
  } while (0)
#define ROUND_C(N)                                \
  do {                                            \
    W0(enc[N]) = W2(enc[N - 2]) ^ W3(enc[N - 1]); \
    W1(enc[N]) = W3(enc[N - 2]) ^ W0(enc[N]);     \
    W2(enc[N]) = W0(enc[N - 1]) ^ W1(enc[N]);     \
    W3(enc[N]) = W1(enc[N - 1]) ^ W2(enc[N]);     \
  } while (0)

      W0(enc[0]) = RBE32(key, 0);
      W1(enc[0]) = RBE32(key, 1);
      W2(enc[0]) = RBE32(key, 2);
      W3(enc[0]) = RBE32(key, 3);
      W0(enc[1]) = RBE32(key, 4);
      W1(enc[1]) = RBE32(key, 5);

      W2(enc[1]) = W0(enc[0]) ^ S0(R(W1(enc[1]))) ^ (0x01 << 24);
      W3(enc[1]) = W1(enc[0]) ^ W2(enc[1]);

      ROUND_C(2);
      ROUND_A(3, 0x02);
      ROUND_B(4, 0x04);
      ROUND_C(5);
      ROUND_A(6, 0x08);
      ROUND_B(7, 0x10);
      ROUND_C(8);
      ROUND_A(9, 0x20);
      ROUND_B(10, 0x40);
      ROUND_C(11);
      ROUND_A(12, 0x80);

      DCHECK_EQ(13U, state->num_rounds);

#undef ROUND_C
#undef ROUND_B
#undef ROUND_A

      break;

    case 32:

#define ROUND_A(N, RCON)                                                \
  do {                                                                  \
    W0(enc[N]) = W0(enc[N - 2]) ^ S0(R(W3(enc[N - 1]))) ^ (RCON << 24); \
    W1(enc[N]) = W1(enc[N - 2]) ^ W0(enc[N]);                           \
    W2(enc[N]) = W2(enc[N - 2]) ^ W1(enc[N]);                           \
    W3(enc[N]) = W3(enc[N - 2]) ^ W2(enc[N]);                           \
  } while (0)
#define ROUND_B(N)                                    \
  do {                                                \
    W0(enc[N]) = W0(enc[N - 2]) ^ S0(W3(enc[N - 1])); \
    W1(enc[N]) = W1(enc[N - 2]) ^ W0(enc[N]);         \
    W2(enc[N]) = W2(enc[N - 2]) ^ W1(enc[N]);         \
    W3(enc[N]) = W3(enc[N - 2]) ^ W2(enc[N]);         \
  } while (0)

      W0(enc[0]) = RBE32(key, 0);
      W1(enc[0]) = RBE32(key, 1);
      W2(enc[0]) = RBE32(key, 2);
      W3(enc[0]) = RBE32(key, 3);
      W0(enc[1]) = RBE32(key, 4);
      W1(enc[1]) = RBE32(key, 5);
      W2(enc[1]) = RBE32(key, 6);
      W3(enc[1]) = RBE32(key, 7);

      ROUND_A(2, 0x01);
      ROUND_B(3);
      ROUND_A(4, 0x02);
      ROUND_B(5);
      ROUND_A(6, 0x04);
      ROUND_B(7);
      ROUND_A(8, 0x08);
      ROUND_B(9);
      ROUND_A(10, 0x10);
      ROUND_B(11);
      ROUND_A(12, 0x20);
      ROUND_B(13);
      ROUND_A(14, 0x40);

      DCHECK_EQ(15U, state->num_rounds);

#undef ROUND_B
#undef ROUND_A

      break;

    default:
      LOG(FATAL) << "BUG! key length = " << len;
  }

  unsigned int n = state->num_rounds - 1;
  dec[0].u32 = enc[n].u32;
  for (unsigned int i = 1; i < n; ++i) {
    W0(dec[i]) = TDS0(W0(enc[n - i]));
    W1(dec[i]) = TDS0(W1(enc[n - i]));
    W2(dec[i]) = TDS0(W2(enc[n - i]));
    W3(dec[i]) = TDS0(W3(enc[n - i]));
  }
  dec[n].u32 = enc[0].u32;
}

void aes_generic_encrypt(const AESState* state, uint8_t* dst,
                         const uint8_t* src, std::size_t len) noexcept {
  AESBlock s, t;
  auto* const enc = state->enc;
  const unsigned int n = state->num_rounds - 1;
  unsigned int i;

  while (len >= 16) {
    // Round 0: AddRoundKey
    W0(s) = W0(enc[0]) ^ RBE32(src, 0);
    W1(s) = W1(enc[0]) ^ RBE32(src, 1);
    W2(s) = W2(enc[0]) ^ RBE32(src, 2);
    W3(s) = W3(enc[0]) ^ RBE32(src, 3);

    // Rounds 1 .. N - 1: SubBytes, ShiftRows, MixColumns, AddRoundKey
    for (i = 1; i < n; ++i) {
      W0(t) = W0(enc[i]) ^ TE(W0(s), W1(s), W2(s), W3(s));
      W1(t) = W1(enc[i]) ^ TE(W1(s), W2(s), W3(s), W0(s));
      W2(t) = W2(enc[i]) ^ TE(W2(s), W3(s), W0(s), W1(s));
      W3(t) = W3(enc[i]) ^ TE(W3(s), W0(s), W1(s), W2(s));
      s.u32 = t.u32;
    }

    // Round N: SubBytes, ShiftRows, AddRoundKey (no MixColumns)
    W0(s) = W0(enc[i]) ^ S0(W0(t), W1(t), W2(t), W3(t));
    W1(s) = W1(enc[i]) ^ S0(W1(t), W2(t), W3(t), W0(t));
    W2(s) = W2(enc[i]) ^ S0(W2(t), W3(t), W0(t), W1(t));
    W3(s) = W3(enc[i]) ^ S0(W3(t), W0(t), W1(t), W2(t));

    WBE32(dst, 0, W0(s));
    WBE32(dst, 1, W1(s));
    WBE32(dst, 2, W2(s));
    WBE32(dst, 3, W3(s));

    src += 16;
    dst += 16;
    len -= 16;
  }
  DCHECK_EQ(len, 0U);
}

void aes_generic_decrypt(const AESState* state, uint8_t* dst,
                         const uint8_t* src, std::size_t len) noexcept {
  AESBlock s, t;
  auto* const dec = state->dec;
  const unsigned int n = state->num_rounds - 1;
  unsigned int i;

  while (len >= 16) {
    // Round 0: AddRoundKey
    W0(s) = W0(dec[0]) ^ RBE32(src, 0);
    W1(s) = W1(dec[0]) ^ RBE32(src, 1);
    W2(s) = W2(dec[0]) ^ RBE32(src, 2);
    W3(s) = W3(dec[0]) ^ RBE32(src, 3);

    // Rounds 1 .. N - 1: InvSubBytes, InvShiftRows, InvMixColumns, AddRoundKey
    for (i = 1; i < n; ++i) {
      W0(t) = W0(dec[i]) ^ TD(W0(s), W3(s), W2(s), W1(s));
      W1(t) = W1(dec[i]) ^ TD(W1(s), W0(s), W3(s), W2(s));
      W2(t) = W2(dec[i]) ^ TD(W2(s), W1(s), W0(s), W3(s));
      W3(t) = W3(dec[i]) ^ TD(W3(s), W2(s), W1(s), W0(s));
      s.u32 = t.u32;
    }

    // Round N: InvSubBytes, InvShiftRows, AddRoundKey (no InvMixColumns)
    W0(s) = W0(dec[i]) ^ S1(W0(t), W3(t), W2(t), W1(t));
    W1(s) = W1(dec[i]) ^ S1(W1(t), W0(t), W3(t), W2(t));
    W2(s) = W2(dec[i]) ^ S1(W2(t), W1(t), W0(t), W3(t));
    W3(s) = W3(dec[i]) ^ S1(W3(t), W2(t), W1(t), W0(t));

    WBE32(dst, 0, W0(s));
    WBE32(dst, 1, W1(s));
    WBE32(dst, 2, W2(s));
    WBE32(dst, 3, W3(s));

    src += 16;
    dst += 16;
    len -= 16;
  }
  DCHECK_EQ(len, 0U);
}

}  // inline namespace implementation
}  // namespace cipher
}  // namespace crypto
