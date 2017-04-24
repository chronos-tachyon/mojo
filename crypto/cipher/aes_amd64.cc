// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/aes_internal.h"

#ifdef __x86_64__
#include <cpuid.h>
#include <emmintrin.h>
#include <tmmintrin.h>
#include <wmmintrin.h>
#endif  // __x86_64__

#include "base/logging.h"

static const uint8_t SHIFT_MASK[16] = {
    0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
};

static const uint8_t HI_MASK[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

namespace crypto {
namespace cipher {
inline namespace implementation {
struct Features {
  bool has_ssse3;
  bool has_aes;
  bool has_pclmul;

  Features() noexcept : has_ssse3(false), has_aes(false), has_pclmul(false) {}
  Features(const Features&) noexcept = default;
  Features(Features&&) noexcept = default;
  Features& operator=(const Features&) noexcept = default;
  Features& operator=(Features&&) noexcept = default;
};

static Features detect_cpuid() noexcept {
  Features result;
#ifdef __x86_64__
  unsigned int ax, bx, cx, dx;
  if (__get_cpuid(1, &ax, &bx, &cx, &dx)) {
    if (cx & bit_SSSE3) {
      VLOG(1) << "SSSE3 detected";
      result.has_ssse3 = true;
    }
    if (cx & bit_AES) {
      VLOG(1) << "AES-NI detected";
      result.has_aes = true;
    }
    if (cx & bit_PCLMUL) {
      VLOG(1) << "PCLMULQDQ detected";
      result.has_pclmul = true;
    }
  }
#endif  // __x86_64__
  return result;
}

static Features features() noexcept {
  static Features value = detect_cpuid();
  return value;
}

bool aes_acceleration_available() noexcept {
  return features().has_ssse3 && features().has_aes;
}

void aes_accelerated_expand_key(AESState* state, const uint8_t* key,
                                std::size_t len) noexcept {
#ifdef __x86_64__
  __m128i xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;
  auto* enc = state->enc;
  auto* dec = state->dec;

  state->num_rounds = (len / 4) + 7;

  // xmm1 <- first 128 bits of key
  // xmm2 <- remaining bits of key
  // xmm3 <- result of AESKEYGENASSIST
  // xmm4 <- shuffled copy of xmm1/xmm2
  // xmm5 <- SHIFT_MASK (d:c:b:a -> c:b:a:0)
  // xmm6 <- temporary
  // xmm7 <- temporary
  // xmm8 <- temporary

  xmm5 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(SHIFT_MASK));

  switch (len) {
    case 16:

// ROUND
//
// Inputs:
//
// xmm1 = {3: D,  <-- newest
//         2: C,
//         1: B,
//         0: A}  <-- oldest
//
// Outputs:
//
// xmm1 = {3: S0(R(D)) ^ RCON ^ D ^ C ^ B ^ A,
//         2: S0(R(D)) ^ RCON ^ C ^ B ^ A,
//         1: S0(R(D)) ^ RCON ^ B ^ A,
//         0: S0(R(D)) ^ RCON ^ A}
//
// Process:
//
// xmm3 = {3: S0(R(D)) ^ RCON,
//         2: don't care,
//         1: don't care,
//         0: don't care}
//
// xmm3 = {3: S0(R(D)) ^ RCON,
//         2: ",
//         1: ",
//         0: "}
//
// xmm4 = xmm1
//
// xmm4 = {3: C,
//         2: B,
//         1: A,
//         0: 0}
//
// xmm1 = {3: D ^ C,
//         2: C ^ B,
//         1: B ^ A,
//         0: A}
//
// ...
//
// xmm1 = {3: S0(R(D)) ^ RCON ^ D ^ C ^ B ^ A,
//         2: S0(R(D)) ^ RCON ^ C ^ B ^ A,
//         1: S0(R(D)) ^ RCON ^ B ^ A,
//         0: S0(R(D)) ^ RCON ^ A}
//
// Goal achieved.

#define ROUND(N, RCON)                                       \
  do {                                                       \
    xmm3 = _mm_aeskeygenassist_si128(xmm1, RCON);            \
    xmm3 = _mm_shuffle_epi32(xmm3, _MM_SHUFFLE(3, 3, 3, 3)); \
    xmm4 = xmm1;                                             \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm1 = _mm_xor_si128(xmm1, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm1 = _mm_xor_si128(xmm1, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm1 = _mm_xor_si128(xmm1, xmm4);                        \
    xmm1 = _mm_xor_si128(xmm1, xmm3);                        \
    _mm_storeu_si128(&enc[N].i128, xmm1);                    \
  } while (0)

      xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key));
      _mm_storeu_si128(&enc[0].i128, xmm1);

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

// ROUND_A
//
// Inputs:
//
// xmm1 = {3: F,  <-- newest
//         2: E,
//         1: D,
//         0: C}
//
// xmm2 = {3: B,
//         2: A,  <-- oldest
//         1: don't care,
//         0: don't care}
//
// Outputs:
//
// xmm1 = unchanged
//
// xmm2 = {3: S0(R(F)) ^ RCON ^ D ^ C ^ B ^ A,
//         2: S0(R(F)) ^ RCON ^ C ^ B ^ A,
//         1: S0(R(F)) ^ RCON ^ B ^ A,
//         0: S0(R(F)) ^ RCON ^ A}
//
// Process:
//
// xmm3 = {3: S0(R(F)) ^ RCON,
//         2: don't care,
//         1: don't care,
//         0: don't care}
//
// xmm3 = {3: S0(R(F)) ^ RCON,
//         2: ",
//         1: ",
//         0: "}
//
// xmm4 = {3: B,
//         2: A,
//         1: B,
//         0: A}
//
// xmm4 = {3: D,
//         2: B,
//         1: C,
//         0: A}
//
// xmm4 = {3: D,
//         2: C,
//         1: B,
//         0: A}
//
// xmm3 = {3: S0(R(F)) ^ RCON ^ D,
//         2: S0(R(F)) ^ RCON ^ C,
//         1: S0(R(F)) ^ RCON ^ B,
//         0: S0(R(F)) ^ RCON ^ A}
//
// xmm4 = {3: C,
//         2: B,
//         1: A,
//         0: 0}
//
// xmm3 = {3: S0(R(F)) ^ RCON ^ D ^ C,
//         2: S0(R(F)) ^ RCON ^ C ^ B,
//         1: S0(R(F)) ^ RCON ^ B ^ A,
//         0: S0(R(F)) ^ RCON ^ A}
//
// ...
//
// xmm2 = xmm3
//
// Goal achieved.

#define ROUND_A(N, RCON)                                     \
  do {                                                       \
    xmm3 = _mm_aeskeygenassist_si128(xmm1, RCON);            \
    xmm3 = _mm_shuffle_epi32(xmm3, _MM_SHUFFLE(3, 3, 3, 3)); \
    xmm4 = _mm_shuffle_epi32(xmm2, _MM_SHUFFLE(3, 2, 3, 2)); \
    xmm4 = _mm_unpacklo_epi32(xmm4, xmm1);                   \
    xmm4 = _mm_shuffle_epi32(xmm4, _MM_SHUFFLE(3, 1, 2, 0)); \
    xmm3 = _mm_xor_si128(xmm3, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm3 = _mm_xor_si128(xmm3, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm3 = _mm_xor_si128(xmm3, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm3 = _mm_xor_si128(xmm3, xmm4);                        \
    xmm2 = xmm3;                                             \
    _mm_storeu_si128(&enc[N].i128, xmm2);                    \
  } while (0)

// ROUND_B
//
// Inputs:
//
// xmm1 = {3: B,
//         2: A,  <-- oldest
//         1: don't care,
//         0: don't care}
//
// xmm2 = {3: F,  <-- newest
//         2: E,
//         1: D,
//         0: C}
//
// Outputs:
//
// xmm1 = {3: F,
//         2: E,
//         1: D,
//         0: C}
//
// xmm2 = {3: S0(R(F ^ B ^ A)) ^ RCON ^ D ^ C,
//         2: S0(R(F ^ B ^ A)) ^ RCON ^ C,
//         1: F ^ B ^ A,
//         0: F ^ A}
//
// Process:
//
// xmm3 = {3: F,
//         2: F,
//         1: F,
//         0: F}
//
// xmm7 = {3: ~0,
//         2: ~0,
//         1:  0,
//         0:  0}
//
// xmm4 = {3: B,
//         2: A,
//         1: 0,
//         0: 0}
//
// xmm3 = {3: F ^ B,
//         2: F ^ A,
//         1: F,
//         0: F}
//
// xmm4 = {3: A,
//         2: 0,
//         1: 0,
//         0: 0}
//
// xmm3 = {3: F ^ B ^ A,
//         2: F ^ A,
//         1: F,
//         0: F}
//
// xmm6 = {3: F ^ B ^ A,
//         2: F ^ A,
//         1: 0,
//         0: 0}
//
// xmm6 = {3: 0,
//         2: 0,
//         1: F ^ B ^ A,
//         0: F ^ A}
//
// xmm3 = {3: S0(R(F ^ B ^ A)) ^ RCON,
//         2: don't care,
//         1: don't care,
//         0: don't care}
//
// xmm3 = {3: S0(R(F ^ B ^ A)) ^ RCON,
//         2: ",
//         1: ",
//         0: "}
//
// xmm8 = {3: D,
//         2: C,
//         1: D,
//         0: C}
//
// xmm8 = {3: D,
//         2: C,
//         1: 0,
//         0: 0}
//
// xmm3 = {3: S0(R(F ^ B ^ A)) ^ RCON ^ D,
//         2: S0(R(F ^ B ^ A)) ^ RCON ^ C,
//         1: S0(R(F ^ B ^ A)) ^ RCON,
//         0: S0(R(F ^ B ^ A)) ^ RCON}
//
// xmm8 = {3: C,
//         2: 0,
//         1: 0,
//         0: 0}
//
// xmm3 = {3: S0(R(F ^ B ^ A)) ^ RCON ^ D ^ C,
//         2: S0(R(F ^ B ^ A)) ^ RCON ^ C,
//         1: S0(R(F ^ B ^ A)) ^ RCON,
//         0: S0(R(F ^ B ^ A)) ^ RCON}
//
// xmm3 = {3: S0(R(F ^ B ^ A)) ^ RCON ^ D ^ C,
//         2: S0(R(F ^ B ^ A)) ^ RCON ^ C,
//         1: 0,
//         0: 0}
//
// xmm6 = {3: S0(R(F ^ B ^ A)) ^ RCON ^ D ^ C,
//         2: S0(R(F ^ B ^ A)) ^ RCON ^ C,
//         1: F ^ B ^ A,
//         0: F ^ A}
//
// xmm1 = xmm2
// xmm2 = xmm6
//
// Goal achieved.

#define ROUND_B(N, RCON)                                               \
  do {                                                                 \
    xmm3 = _mm_shuffle_epi32(xmm2, _MM_SHUFFLE(3, 3, 3, 3));           \
    xmm7 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(HI_MASK)); \
    xmm4 = _mm_and_si128(xmm1, xmm7);                                  \
    xmm3 = _mm_xor_si128(xmm3, xmm4);                                  \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                               \
    xmm3 = _mm_xor_si128(xmm3, xmm4);                                  \
    xmm6 = _mm_and_si128(xmm3, xmm7);                                  \
    xmm6 = _mm_shuffle_epi32(xmm6, _MM_SHUFFLE(1, 0, 3, 2));           \
    xmm3 = _mm_aeskeygenassist_si128(xmm3, RCON);                      \
    xmm3 = _mm_shuffle_epi32(xmm3, _MM_SHUFFLE(3, 3, 3, 3));           \
    xmm8 = _mm_shuffle_epi32(xmm2, _MM_SHUFFLE(1, 0, 1, 0));           \
    xmm8 = _mm_and_si128(xmm8, xmm7);                                  \
    xmm3 = _mm_xor_si128(xmm3, xmm8);                                  \
    xmm8 = _mm_shuffle_epi8(xmm8, xmm5);                               \
    xmm3 = _mm_xor_si128(xmm3, xmm8);                                  \
    xmm3 = _mm_and_si128(xmm3, xmm7);                                  \
    xmm6 = _mm_or_si128(xmm6, xmm3);                                   \
    xmm1 = xmm2;                                                       \
    xmm2 = xmm6;                                                       \
    _mm_storeu_si128(&enc[N].i128, xmm6);                              \
  } while (0)

// ROUND_C
//
// Inputs:
//
// xmm1 = {3: B,
//         2: A,  <-- oldest
//         1: don't care,
//         0: don't care}
//
// xmm2 = {3: F,  <-- newest
//         2: E,
//         1: D,
//         0: C}
//
// Outputs:
//
// xmm1 = {3: F ^ D ^ C ^ B ^ A,
//         2: F ^ C ^ B ^ A,
//         1: F ^ B ^ A,
//         0: F ^ A}
//
// xmm2 = unchanged
//
// Process:
//
// xmm3 = {3: F,
//         2: F,
//         1: F,
//         0: F}
//
// xmm4 = {3: B,
//         2: A,
//         1: B,
//         0: A}
//
// xmm4 = {3: D,
//         2: B,
//         1: C,
//         0: A}
//
// xmm4 = {3: D,
//         2: C,
//         1: B,
//         0: A}
//
// xmm3 = {3: F ^ D,
//         2: F ^ C,
//         1: F ^ B,
//         0: F ^ A}
//
// xmm4 = {3: C,
//         2: B,
//         1: A,
//         0: 0}
//
// xmm3 = {3: F ^ D ^ C,
//         2: F ^ C ^ B,
//         1: F ^ B ^ A,
//         0: F ^ A}
//
// ...
//
// xmm1 = xmm3
//
// Goal achieved.

#define ROUND_C(N)                                           \
  do {                                                       \
    xmm3 = _mm_shuffle_epi32(xmm2, _MM_SHUFFLE(3, 3, 3, 3)); \
    xmm4 = _mm_shuffle_epi32(xmm1, _MM_SHUFFLE(3, 2, 3, 2)); \
    xmm4 = _mm_unpacklo_epi32(xmm4, xmm2);                   \
    xmm4 = _mm_shuffle_epi32(xmm4, _MM_SHUFFLE(3, 1, 2, 0)); \
    xmm3 = _mm_xor_si128(xmm3, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm3 = _mm_xor_si128(xmm3, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm3 = _mm_xor_si128(xmm3, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm3 = _mm_xor_si128(xmm3, xmm4);                        \
    xmm1 = xmm3;                                             \
    _mm_storeu_si128(&enc[N].i128, xmm1);                    \
  } while (0)

      xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key));
      xmm2 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(key + 16));
      //
      // xmm1 = {3: K3,
      //         2: K2,
      //         1: K1,
      //         0: K0}
      //
      // xmm2 = {3:  0,
      //         2:  0,
      //         1: K5,
      //         0: K4}
      //
      // Goal:
      //
      // xmm1 = {3: K3,
      //         2: K2,
      //         1: K1,
      //         0: K0}
      //
      // xmm2 = {3: K1 ^ K0 ^ S0(R(K5)) ^ 0x01,
      //         2: K0 ^ S0(R(K5)) ^ 0x01,
      //         1: K5,
      //         0: K4}
      //
      xmm3 = _mm_aeskeygenassist_si128(xmm2, 0x01);
      xmm3 = _mm_shuffle_epi32(xmm3, _MM_SHUFFLE(1, 1, 1, 1));
      //
      // xmm3 = {3: S0(R(K5)) ^ 0x01,
      //         2: ",
      //         1: ",
      //         0: "}
      //
      xmm6 = _mm_loadl_epi64(&xmm1);
      xmm6 = _mm_shuffle_epi32(xmm6, _MM_SHUFFLE(1, 0, 3, 2));
      xmm2 = _mm_xor_si128(xmm2, xmm6);
      //
      // xmm6 = {3: K1,
      //         2: K0,
      //         1:  0,
      //         0:  0}
      //
      // xmm2 = {3: K1,
      //         2: K0,
      //         1: K5,
      //         0: K4}
      //
      xmm6 = _mm_shuffle_epi8(xmm6, xmm5);
      xmm2 = _mm_xor_si128(xmm2, xmm6);
      //
      // xmm6 = {3: K0,
      //         2:  0,
      //         1:  0,
      //         0:  0}
      //
      // xmm2 = {3: K1 ^ K0,
      //         2: K0,
      //         1: K5,
      //         0: K4}
      //
      xmm7 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(HI_MASK));
      xmm3 = _mm_and_si128(xmm3, xmm7);
      xmm2 = _mm_xor_si128(xmm2, xmm3);
      //
      // xmm7 = {3: ~0,
      //         2: ~0,
      //         1:  0,
      //         0:  0}
      //
      // xmm3 = {3: S0(R(K5)) ^ 0x01,
      //         2: S0(R(K5)) ^ 0x01,
      //         1:                0,
      //         0:                0}
      //
      // xmm2 = {3: K1 ^ K0 ^ S0(R(K5)) ^ 0x01,
      //         2: K0 ^ S0(R(K5)) ^ 0x01,
      //         1: K5,
      //         0: K4}
      //
      // Goal achieved.
      //
      _mm_storeu_si128(&enc[0].i128, xmm1);
      _mm_storeu_si128(&enc[1].i128, xmm2);

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

// ROUND_A
//
// Inputs:
//
// xmm1 = {3: D,
//         2: C,
//         1: B,
//         0: A}  <-- oldest
//
// xmm2 = {3: H,  <-- newest
//         2: G,
//         1: F,
//         0: E}
//
// Outputs:
//
// xmm1 = {3: S0(R(H)) ^ RCON ^ D ^ C ^ B ^ A,
//         2: S0(R(H)) ^ RCON ^ C ^ B ^ A,
//         1: S0(R(H)) ^ RCON ^ B ^ A,
//         0: S0(R(H)) ^ RCON ^ A}
//
// xmm2 = unchanged
//
// Process:
//
// xmm3 = {3: S0(R(H)) ^ RCON,
//         2: don't care,
//         1: don't care,
//         0: don't care}
//
// xmm3 = {3: S0(R(H)) ^ RCON,
//         2: ",
//         1: ",
//         0: "}
//
// xmm4 = xmm1
//
// xmm4 = {3: C,
//         2: B,
//         1: A,
//         0: 0}
//
// xmm1 = {3: D ^ C,
//         2: C ^ B,
//         1: B ^ A,
//         0: A}
//
// ...
//
// xmm1 = {3: S0(R(H)) ^ RCON ^ D ^ C ^ B ^ A,
//         2: S0(R(H)) ^ RCON ^ C ^ B ^ A,
//         1: S0(R(H)) ^ RCON ^ B ^ A,
//         0: S0(R(H)) ^ RCON ^ A}
//
// Goal achieved.

#define ROUND_A(N, RCON)                                     \
  do {                                                       \
    xmm3 = _mm_aeskeygenassist_si128(xmm2, RCON);            \
    xmm3 = _mm_shuffle_epi32(xmm3, _MM_SHUFFLE(3, 3, 3, 3)); \
    xmm4 = xmm1;                                             \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm1 = _mm_xor_si128(xmm1, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm1 = _mm_xor_si128(xmm1, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm1 = _mm_xor_si128(xmm1, xmm4);                        \
    xmm1 = _mm_xor_si128(xmm1, xmm3);                        \
    _mm_storeu_si128(&enc[N].i128, xmm1);                    \
  } while (0)

// ROUND_B
//
// Inputs:
//
// xmm1 = {3: H,  <-- newest
//         2: G,
//         1: F,
//         0: E}
//
// xmm2 = {3: D,
//         2: C,
//         1: B,
//         0: A}  <-- oldest
//
// Outputs:
//
// xmm1 = unchanged
//
// xmm2 = {3: S0(H) ^ D ^ C ^ B ^ A,
//         2: S0(H) ^ C ^ B ^ A,
//         1: S0(H) ^ B ^ A,
//         0: S0(H) ^ A}
//
// Process:
//
// xmm3 = {3: don't care,
//         2: S0(H),
//         1: don't care,
//         0: don't care}
//
// xmm3 = {3: S0(H),
//         2: ",
//         1: ",
//         0: "}
//
// xmm4 = xmm2
//
// xmm4 = {3: C,
//         2: B,
//         1: A,
//         0: 0}
//
// xmm1 = {3: D ^ C,
//         2: C ^ B,
//         1: B ^ A,
//         0: A}
//
// ...
//
// xmm2 = {3: S0(H) ^ D ^ C ^ B ^ A,
//         2: S0(H) ^ C ^ B ^ A,
//         1: S0(H) ^ B ^ A,
//         0: S0(H) ^ A}
//
// Goal achieved.

#define ROUND_B(N)                                           \
  do {                                                       \
    xmm3 = _mm_aeskeygenassist_si128(xmm1, 0x00);            \
    xmm3 = _mm_shuffle_epi32(xmm3, _MM_SHUFFLE(2, 2, 2, 2)); \
    xmm4 = xmm2;                                             \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm2 = _mm_xor_si128(xmm2, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm2 = _mm_xor_si128(xmm2, xmm4);                        \
    xmm4 = _mm_shuffle_epi8(xmm4, xmm5);                     \
    xmm2 = _mm_xor_si128(xmm2, xmm4);                        \
    xmm2 = _mm_xor_si128(xmm2, xmm3);                        \
    _mm_storeu_si128(&enc[N].i128, xmm2);                    \
  } while (0)

      xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key));
      xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key + 16));
      _mm_storeu_si128(&enc[0].i128, xmm1);
      _mm_storeu_si128(&enc[1].i128, xmm2);

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
  dec[0].i128 = enc[n].i128;
  for (unsigned int i = 1; i < n; ++i) {
    xmm1 = _mm_loadu_si128(&enc[n - i].i128);
    xmm1 = _mm_aesimc_si128(xmm1);
    _mm_storeu_si128(&dec[i].i128, xmm1);
  }
  dec[n].i128 = enc[0].i128;
#endif  // __x86_64__
}

void aes_accelerated_encrypt(const AESState* state, uint8_t* dst,
                             const uint8_t* src, std::size_t len) noexcept {
#ifdef __x86_64__
  __m128i s, t, u, v;
  auto* const enc = state->enc;
  unsigned int i;

  while (len >= 64) {
    s = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
    t = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 16));
    u = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 32));
    v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 48));

    s = _mm_xor_si128(s, enc[0].i128);
    t = _mm_xor_si128(t, enc[0].i128);
    u = _mm_xor_si128(u, enc[0].i128);
    v = _mm_xor_si128(v, enc[0].i128);

#define ROUND                             \
  do {                                    \
    s = _mm_aesenc_si128(s, enc[i].i128); \
    t = _mm_aesenc_si128(t, enc[i].i128); \
    u = _mm_aesenc_si128(u, enc[i].i128); \
    v = _mm_aesenc_si128(v, enc[i].i128); \
    ++i;                                  \
  } while (0)

    i = 1;
    switch (state->num_rounds) {
      case 15:
        ROUND;
        ROUND;

      // fallthrough
      case 13:
        ROUND;
        ROUND;

      // fallthrough
      case 11:
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        s = _mm_aesenclast_si128(s, enc[i].i128);
        t = _mm_aesenclast_si128(t, enc[i].i128);
        u = _mm_aesenclast_si128(u, enc[i].i128);
        v = _mm_aesenclast_si128(v, enc[i].i128);
        break;

      default:
        LOG(FATAL) << "BUG! num_rounds = " << state->num_rounds;
    }

#undef ROUND

    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), s);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), t);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), u);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), v);

    src += 64;
    dst += 64;
    len -= 64;
  }

  while (len >= 16) {
    s = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
    s = _mm_xor_si128(s, enc[0].i128);

#define ROUND                             \
  do {                                    \
    s = _mm_aesenc_si128(s, enc[i].i128); \
    ++i;                                  \
  } while (0)

    i = 1;
    switch (state->num_rounds) {
      case 15:
        ROUND;
        ROUND;

      // fallthrough
      case 13:
        ROUND;
        ROUND;

      // fallthrough
      case 11:
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        s = _mm_aesenclast_si128(s, enc[i].i128);
        break;

      default:
        LOG(FATAL) << "BUG! num_rounds = " << state->num_rounds;
    }

#undef ROUND

    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), s);

    src += 16;
    dst += 16;
    len -= 16;
  }
  DCHECK_EQ(len, 0U);
#endif  // __x86_64__
}

void aes_accelerated_decrypt(const AESState* state, uint8_t* dst,
                             const uint8_t* src, std::size_t len) noexcept {
#ifdef __x86_64__
  __m128i s, t, u, v;
  auto* const dec = state->dec;
  unsigned int i;

  while (len >= 64) {
    s = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
    t = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 16));
    u = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 32));
    v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 48));

    s = _mm_xor_si128(s, dec[0].i128);
    t = _mm_xor_si128(t, dec[0].i128);
    u = _mm_xor_si128(u, dec[0].i128);
    v = _mm_xor_si128(v, dec[0].i128);

#define ROUND                             \
  do {                                    \
    s = _mm_aesdec_si128(s, dec[i].i128); \
    t = _mm_aesdec_si128(t, dec[i].i128); \
    u = _mm_aesdec_si128(u, dec[i].i128); \
    v = _mm_aesdec_si128(v, dec[i].i128); \
    ++i;                                  \
  } while (0)

    i = 1;
    switch (state->num_rounds) {
      case 15:
        ROUND;
        ROUND;

      // fallthrough
      case 13:
        ROUND;
        ROUND;

      // fallthrough
      case 11:
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        s = _mm_aesdeclast_si128(s, dec[i].i128);
        t = _mm_aesdeclast_si128(t, dec[i].i128);
        u = _mm_aesdeclast_si128(u, dec[i].i128);
        v = _mm_aesdeclast_si128(v, dec[i].i128);
        break;

      default:
        LOG(FATAL) << "BUG! num_rounds = " << state->num_rounds;
    }

#undef ROUND

    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), s);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), t);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), u);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), v);

    src += 64;
    dst += 64;
    len -= 64;
  }

  while (len >= 16) {
    s = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
    s = _mm_xor_si128(s, dec[0].i128);

#define ROUND                             \
  do {                                    \
    s = _mm_aesdec_si128(s, dec[i].i128); \
    ++i;                                  \
  } while (0)

    i = 1;
    switch (state->num_rounds) {
      case 15:
        ROUND;
        ROUND;

      // fallthrough
      case 13:
        ROUND;
        ROUND;

      // fallthrough
      case 11:
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        ROUND;
        s = _mm_aesdeclast_si128(s, dec[i].i128);
        break;

      default:
        LOG(FATAL) << "BUG! num_rounds = " << state->num_rounds;
    }

#undef ROUND

    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), s);

    src += 16;
    dst += 16;
    len -= 16;
  }
  DCHECK_EQ(len, 0U);
#endif  // __x86_64__
}

}  // inline namespace implementation
}  // namespace cipher
}  // namespace crypto
