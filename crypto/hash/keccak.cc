// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/keccak.h"

#include "base/logging.h"

#define Aba state[0]
#define Abe state[1]
#define Abi state[2]
#define Abo state[3]
#define Abu state[4]
#define Aga state[5]
#define Age state[6]
#define Agi state[7]
#define Ago state[8]
#define Agu state[9]
#define Aka state[10]
#define Ake state[11]
#define Aki state[12]
#define Ako state[13]
#define Aku state[14]
#define Ama state[15]
#define Ame state[16]
#define Ami state[17]
#define Amo state[18]
#define Amu state[19]
#define Asa state[20]
#define Ase state[21]
#define Asi state[22]
#define Aso state[23]
#define Asu state[24]

static constexpr bool OPT =
#if defined(__i386__) || defined(__x86_64__)
    true;
#else
    false;
#endif

static const uint64_t ROUND_CONSTANTS[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};

static inline __attribute__((always_inline)) uint64_t ROL64(uint64_t x,
                                                            unsigned int c) {
  c &= 63;
  return (x << c) | (x >> (64 - c));
}

static inline __attribute__((always_inline)) uint64_t X(const uint8_t* ptr,
                                                        unsigned int index) {
  if (OPT) {
    const uint64_t* ptr64 = reinterpret_cast<const uint64_t*>(ptr);
    return ptr64[index];
  } else {
    uint64_t byte0 = ptr[(index * 8) + 0];
    uint64_t byte1 = ptr[(index * 8) + 1];
    uint64_t byte2 = ptr[(index * 8) + 2];
    uint64_t byte3 = ptr[(index * 8) + 3];
    uint64_t byte4 = ptr[(index * 8) + 4];
    uint64_t byte5 = ptr[(index * 8) + 5];
    uint64_t byte6 = ptr[(index * 8) + 6];
    uint64_t byte7 = ptr[(index * 8) + 7];
    return byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24) |
           (byte4 << 32) | (byte5 << 40) | (byte6 << 48) | (byte7 << 56);
  }
}

static inline __attribute__((always_inline)) void Y(uint8_t* ptr,
                                                    unsigned int index,
                                                    uint64_t value) {
  if (OPT) {
    uint64_t* ptr64 = reinterpret_cast<uint64_t*>(ptr);
    ptr64[index] = value;
  } else {
    ptr[(index * 8) + 0] = (value & 0xffU);
    ptr[(index * 8) + 1] = ((value >> 8) & 0xffU);
    ptr[(index * 8) + 2] = ((value >> 16) & 0xffU);
    ptr[(index * 8) + 3] = ((value >> 24) & 0xffU);
    ptr[(index * 8) + 4] = ((value >> 32) & 0xffU);
    ptr[(index * 8) + 5] = ((value >> 40) & 0xffU);
    ptr[(index * 8) + 6] = ((value >> 48) & 0xffU);
    ptr[(index * 8) + 7] = ((value >> 56) & 0xffU);
  }
}

namespace crypto {
namespace hash {

void keccak_f1600_xor_in(uint64_t* state, const uint8_t* in, unsigned int len) {
  DCHECK((len & 7) == 0);
  DCHECK_LE(len, 25U * 8U);

  unsigned int i = len >> 3;
  while (i > 0) {
    --i;
    state[i] ^= X(in, i);
  }
}

void keccak_f1600_copy_out(uint8_t* out, unsigned int len,
                           const uint64_t* state) {
  DCHECK((len & 7) == 0);
  DCHECK_LE(len, 25U * 8U);

  unsigned int i = len >> 3;
  while (i > 0) {
    --i;
    Y(out, i, state[i]);
  }
}

void keccak_f1600_permute(uint64_t* state) {
  uint64_t Ba, Be, Bi, Bo, Bu;
  uint64_t Ca, Ce, Ci, Co, Cu;
  uint64_t Da, De, Di, Do, Du;
  for (unsigned int i = 0; i < 24; i += 4) {
    Ca = Aba ^ Aga ^ Aka ^ Ama ^ Asa;
    Ce = Abe ^ Age ^ Ake ^ Ame ^ Ase;
    Ci = Abi ^ Agi ^ Aki ^ Ami ^ Asi;
    Co = Abo ^ Ago ^ Ako ^ Amo ^ Aso;
    Cu = Abu ^ Agu ^ Aku ^ Amu ^ Asu;
    Da = Cu ^ ROL64(Ce, 1);
    De = Ca ^ ROL64(Ci, 1);
    Di = Ce ^ ROL64(Co, 1);
    Do = Ci ^ ROL64(Cu, 1);
    Du = Co ^ ROL64(Ca, 1);

    Ba = (Aba ^ Da);
    Be = ROL64((Age ^ De), 44);
    Bi = ROL64((Aki ^ Di), 43);
    Bo = ROL64((Amo ^ Do), 21);
    Bu = ROL64((Asu ^ Du), 14);
    Aba = Ba ^ ((~Be) & Bi);
    Aba ^= ROUND_CONSTANTS[i + 0];
    Age = Be ^ ((~Bi) & Bo);
    Aki = Bi ^ ((~Bo) & Bu);
    Amo = Bo ^ ((~Bu) & Ba);
    Asu = Bu ^ ((~Ba) & Be);

    Bi = ROL64((Aka ^ Da), 3);
    Bo = ROL64((Ame ^ De), 45);
    Bu = ROL64((Asi ^ Di), 61);
    Ba = ROL64((Abo ^ Do), 28);
    Be = ROL64((Agu ^ Du), 20);
    Aka = Ba ^ ((~Be) & Bi);
    Ame = Be ^ ((~Bi) & Bo);
    Asi = Bi ^ ((~Bo) & Bu);
    Abo = Bo ^ ((~Bu) & Ba);
    Agu = Bu ^ ((~Ba) & Be);

    Bu = ROL64((Asa ^ Da), 18);
    Ba = ROL64((Abe ^ De), 1);
    Be = ROL64((Agi ^ Di), 6);
    Bi = ROL64((Ako ^ Do), 25);
    Bo = ROL64((Amu ^ Du), 8);
    Asa = Ba ^ ((~Be) & Bi);
    Abe = Be ^ ((~Bi) & Bo);
    Agi = Bi ^ ((~Bo) & Bu);
    Ako = Bo ^ ((~Bu) & Ba);
    Amu = Bu ^ ((~Ba) & Be);

    Be = ROL64((Aga ^ Da), 36);
    Bi = ROL64((Ake ^ De), 10);
    Bo = ROL64((Ami ^ Di), 15);
    Bu = ROL64((Aso ^ Do), 56);
    Ba = ROL64((Abu ^ Du), 27);
    Aga = Ba ^ ((~Be) & Bi);
    Ake = Be ^ ((~Bi) & Bo);
    Ami = Bi ^ ((~Bo) & Bu);
    Aso = Bo ^ ((~Bu) & Ba);
    Abu = Bu ^ ((~Ba) & Be);

    Bo = ROL64((Ama ^ Da), 41);
    Bu = ROL64((Ase ^ De), 2);
    Ba = ROL64((Abi ^ Di), 62);
    Be = ROL64((Ago ^ Do), 55);
    Bi = ROL64((Aku ^ Du), 39);
    Ama = Ba ^ ((~Be) & Bi);
    Ase = Be ^ ((~Bi) & Bo);
    Abi = Bi ^ ((~Bo) & Bu);
    Ago = Bo ^ ((~Bu) & Ba);
    Aku = Bu ^ ((~Ba) & Be);

    Ca = Aba ^ Aka ^ Asa ^ Aga ^ Ama;
    Ce = Age ^ Ame ^ Abe ^ Ake ^ Ase;
    Ci = Aki ^ Asi ^ Agi ^ Ami ^ Abi;
    Co = Amo ^ Abo ^ Ako ^ Aso ^ Ago;
    Cu = Asu ^ Agu ^ Amu ^ Abu ^ Aku;
    Da = Cu ^ ROL64(Ce, 1);
    De = Ca ^ ROL64(Ci, 1);
    Di = Ce ^ ROL64(Co, 1);
    Do = Ci ^ ROL64(Cu, 1);
    Du = Co ^ ROL64(Ca, 1);

    Ba = (Aba ^ Da);
    Be = ROL64((Ame ^ De), 44);
    Bi = ROL64((Agi ^ Di), 43);
    Bo = ROL64((Aso ^ Do), 21);
    Bu = ROL64((Aku ^ Du), 14);
    Aba = Ba ^ ((~Be) & Bi);
    Aba ^= ROUND_CONSTANTS[i + 1];
    Ame = Be ^ ((~Bi) & Bo);
    Agi = Bi ^ ((~Bo) & Bu);
    Aso = Bo ^ ((~Bu) & Ba);
    Aku = Bu ^ ((~Ba) & Be);

    Bi = ROL64((Asa ^ Da), 3);
    Bo = ROL64((Ake ^ De), 45);
    Bu = ROL64((Abi ^ Di), 61);
    Ba = ROL64((Amo ^ Do), 28);
    Be = ROL64((Agu ^ Du), 20);
    Asa = Ba ^ ((~Be) & Bi);
    Ake = Be ^ ((~Bi) & Bo);
    Abi = Bi ^ ((~Bo) & Bu);
    Amo = Bo ^ ((~Bu) & Ba);
    Agu = Bu ^ ((~Ba) & Be);

    Bu = ROL64((Ama ^ Da), 18);
    Ba = ROL64((Age ^ De), 1);
    Be = ROL64((Asi ^ Di), 6);
    Bi = ROL64((Ako ^ Do), 25);
    Bo = ROL64((Abu ^ Du), 8);
    Ama = Ba ^ ((~Be) & Bi);
    Age = Be ^ ((~Bi) & Bo);
    Asi = Bi ^ ((~Bo) & Bu);
    Ako = Bo ^ ((~Bu) & Ba);
    Abu = Bu ^ ((~Ba) & Be);

    Be = ROL64((Aka ^ Da), 36);
    Bi = ROL64((Abe ^ De), 10);
    Bo = ROL64((Ami ^ Di), 15);
    Bu = ROL64((Ago ^ Do), 56);
    Ba = ROL64((Asu ^ Du), 27);
    Aka = Ba ^ ((~Be) & Bi);
    Abe = Be ^ ((~Bi) & Bo);
    Ami = Bi ^ ((~Bo) & Bu);
    Ago = Bo ^ ((~Bu) & Ba);
    Asu = Bu ^ ((~Ba) & Be);

    Bo = ROL64((Aga ^ Da), 41);
    Bu = ROL64((Ase ^ De), 2);
    Ba = ROL64((Aki ^ Di), 62);
    Be = ROL64((Abo ^ Do), 55);
    Bi = ROL64((Amu ^ Du), 39);
    Aga = Ba ^ ((~Be) & Bi);
    Ase = Be ^ ((~Bi) & Bo);
    Aki = Bi ^ ((~Bo) & Bu);
    Abo = Bo ^ ((~Bu) & Ba);
    Amu = Bu ^ ((~Ba) & Be);

    Ca = Aba ^ Asa ^ Ama ^ Aka ^ Aga;
    Ce = Ame ^ Ake ^ Age ^ Abe ^ Ase;
    Ci = Agi ^ Abi ^ Asi ^ Ami ^ Aki;
    Co = Aso ^ Amo ^ Ako ^ Ago ^ Abo;
    Cu = Aku ^ Agu ^ Abu ^ Asu ^ Amu;
    Da = Cu ^ ROL64(Ce, 1);
    De = Ca ^ ROL64(Ci, 1);
    Di = Ce ^ ROL64(Co, 1);
    Do = Ci ^ ROL64(Cu, 1);
    Du = Co ^ ROL64(Ca, 1);

    Ba = (Aba ^ Da);
    Be = ROL64((Ake ^ De), 44);
    Bi = ROL64((Asi ^ Di), 43);
    Bo = ROL64((Ago ^ Do), 21);
    Bu = ROL64((Amu ^ Du), 14);
    Aba = Ba ^ ((~Be) & Bi);
    Aba ^= ROUND_CONSTANTS[i + 2];
    Ake = Be ^ ((~Bi) & Bo);
    Asi = Bi ^ ((~Bo) & Bu);
    Ago = Bo ^ ((~Bu) & Ba);
    Amu = Bu ^ ((~Ba) & Be);

    Bi = ROL64((Ama ^ Da), 3);
    Bo = ROL64((Abe ^ De), 45);
    Bu = ROL64((Aki ^ Di), 61);
    Ba = ROL64((Aso ^ Do), 28);
    Be = ROL64((Agu ^ Du), 20);
    Ama = Ba ^ ((~Be) & Bi);
    Abe = Be ^ ((~Bi) & Bo);
    Aki = Bi ^ ((~Bo) & Bu);
    Aso = Bo ^ ((~Bu) & Ba);
    Agu = Bu ^ ((~Ba) & Be);

    Bu = ROL64((Aga ^ Da), 18);
    Ba = ROL64((Ame ^ De), 1);
    Be = ROL64((Abi ^ Di), 6);
    Bi = ROL64((Ako ^ Do), 25);
    Bo = ROL64((Asu ^ Du), 8);
    Aga = Ba ^ ((~Be) & Bi);
    Ame = Be ^ ((~Bi) & Bo);
    Abi = Bi ^ ((~Bo) & Bu);
    Ako = Bo ^ ((~Bu) & Ba);
    Asu = Bu ^ ((~Ba) & Be);

    Be = ROL64((Asa ^ Da), 36);
    Bi = ROL64((Age ^ De), 10);
    Bo = ROL64((Ami ^ Di), 15);
    Bu = ROL64((Abo ^ Do), 56);
    Ba = ROL64((Aku ^ Du), 27);
    Asa = Ba ^ ((~Be) & Bi);
    Age = Be ^ ((~Bi) & Bo);
    Ami = Bi ^ ((~Bo) & Bu);
    Abo = Bo ^ ((~Bu) & Ba);
    Aku = Bu ^ ((~Ba) & Be);

    Bo = ROL64((Aka ^ Da), 41);
    Bu = ROL64((Ase ^ De), 2);
    Ba = ROL64((Agi ^ Di), 62);
    Be = ROL64((Amo ^ Do), 55);
    Bi = ROL64((Abu ^ Du), 39);
    Aka = Ba ^ ((~Be) & Bi);
    Ase = Be ^ ((~Bi) & Bo);
    Agi = Bi ^ ((~Bo) & Bu);
    Amo = Bo ^ ((~Bu) & Ba);
    Abu = Bu ^ ((~Ba) & Be);

    Ca = Aba ^ Ama ^ Aga ^ Asa ^ Aka;
    Ce = Ake ^ Abe ^ Ame ^ Age ^ Ase;
    Ci = Asi ^ Aki ^ Abi ^ Ami ^ Agi;
    Co = Ago ^ Aso ^ Ako ^ Abo ^ Amo;
    Cu = Amu ^ Agu ^ Asu ^ Aku ^ Abu;
    Da = Cu ^ ROL64(Ce, 1);
    De = Ca ^ ROL64(Ci, 1);
    Di = Ce ^ ROL64(Co, 1);
    Do = Ci ^ ROL64(Cu, 1);
    Du = Co ^ ROL64(Ca, 1);

    Ba = (Aba ^ Da);
    Be = ROL64((Abe ^ De), 44);
    Bi = ROL64((Abi ^ Di), 43);
    Bo = ROL64((Abo ^ Do), 21);
    Bu = ROL64((Abu ^ Du), 14);
    Aba = Ba ^ ((~Be) & Bi);
    Aba ^= ROUND_CONSTANTS[i + 3];
    Abe = Be ^ ((~Bi) & Bo);
    Abi = Bi ^ ((~Bo) & Bu);
    Abo = Bo ^ ((~Bu) & Ba);
    Abu = Bu ^ ((~Ba) & Be);

    Bi = ROL64((Aga ^ Da), 3);
    Bo = ROL64((Age ^ De), 45);
    Bu = ROL64((Agi ^ Di), 61);
    Ba = ROL64((Ago ^ Do), 28);
    Be = ROL64((Agu ^ Du), 20);
    Aga = Ba ^ ((~Be) & Bi);
    Age = Be ^ ((~Bi) & Bo);
    Agi = Bi ^ ((~Bo) & Bu);
    Ago = Bo ^ ((~Bu) & Ba);
    Agu = Bu ^ ((~Ba) & Be);

    Bu = ROL64((Aka ^ Da), 18);
    Ba = ROL64((Ake ^ De), 1);
    Be = ROL64((Aki ^ Di), 6);
    Bi = ROL64((Ako ^ Do), 25);
    Bo = ROL64((Aku ^ Du), 8);
    Aka = Ba ^ ((~Be) & Bi);
    Ake = Be ^ ((~Bi) & Bo);
    Aki = Bi ^ ((~Bo) & Bu);
    Ako = Bo ^ ((~Bu) & Ba);
    Aku = Bu ^ ((~Ba) & Be);

    Be = ROL64((Ama ^ Da), 36);
    Bi = ROL64((Ame ^ De), 10);
    Bo = ROL64((Ami ^ Di), 15);
    Bu = ROL64((Amo ^ Do), 56);
    Ba = ROL64((Amu ^ Du), 27);
    Ama = Ba ^ ((~Be) & Bi);
    Ame = Be ^ ((~Bi) & Bo);
    Ami = Bi ^ ((~Bo) & Bu);
    Amo = Bo ^ ((~Bu) & Ba);
    Amu = Bu ^ ((~Ba) & Be);

    Bo = ROL64((Asa ^ Da), 41);
    Bu = ROL64((Ase ^ De), 2);
    Ba = ROL64((Asi ^ Di), 62);
    Be = ROL64((Aso ^ Do), 55);
    Bi = ROL64((Asu ^ Du), 39);
    Asa = Ba ^ ((~Be) & Bi);
    Ase = Be ^ ((~Bi) & Bo);
    Asi = Bi ^ ((~Bo) & Bu);
    Aso = Bo ^ ((~Bu) & Ba);
    Asu = Bu ^ ((~Ba) & Be);
  }
}
}  // namespace hash
}  // namespace crypto
