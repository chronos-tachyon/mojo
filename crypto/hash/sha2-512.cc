// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/sha2.h"

#include "base/logging.h"
#include "crypto/primitives.h"

using crypto::primitives::ROR64;
using crypto::primitives::RBE64;
using crypto::primitives::WBE64;

static const uint64_t SHA512_224_H[8] = {
    0x8c3d37c819544da2ULL, 0x73e1996689dcd4d6ULL, 0x1dfab7ae32ff9c82ULL,
    0x679dd514582f9fcfULL, 0x0f6d2b697bd44da8ULL, 0x77e36f7304c48942ULL,
    0x3f9d85a86a1d36c8ULL, 0x1112e6ad91d692a1ULL,
};

static const uint64_t SHA512_256_H[8] = {
    0x22312194fc2bf72cULL, 0x9f555fa3c84c64c2ULL, 0x2393b86b6f53b151ULL,
    0x963877195940eabdULL, 0x96283ee2a88effe3ULL, 0xbe5e1e2553863992ULL,
    0x2b0199fc2c85b8aaULL, 0x0eb72ddc81c52ca2ULL,
};

static const uint64_t SHA384_H[8] = {
    0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL, 0x9159015a3070dd17ULL,
    0x152fecd8f70e5939ULL, 0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL,
    0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL,
};

static const uint64_t SHA512_H[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL,
    0xa54ff53a5f1d36f1ULL, 0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
};

static const uint64_t K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL,
    0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
    0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL,
    0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
    0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
    0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL,
    0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL,
    0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL,
    0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL,
    0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
    0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
    0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
    0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
    0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL,
    0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
    0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
    0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL,
    0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
    0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};

namespace crypto {
namespace hash {

inline namespace implementation {
class SHA512Hasher : public Hasher {
 public:
  struct Raw {
    uint8_t x[SHA512_BLOCKSIZE];
    uint64_t h[8];
    uint64_t len;
    uint8_t nx;
    bool finalized;
  };

  enum class ID : unsigned int {
    sha512_224 = 1,
    sha512_256 = 2,
    sha384 = 3,
    sha512 = 4,
  };

  explicit SHA512Hasher(ID id) noexcept;
  SHA512Hasher(const SHA512Hasher& src) noexcept;

  uint16_t block_size() const noexcept override { return SHA512_BLOCKSIZE; }
  uint16_t output_size() const noexcept override;
  bool is_sponge() const noexcept override { return false; }

  std::unique_ptr<Hasher> copy() const override;
  void reset() override;
  void write(base::Bytes in) override;
  void finalize() override;
  void sum(base::MutableBytes out) override;

 private:
  void block(const uint8_t* ptr, uint64_t len);

  Raw raw_;
  ID id_;
};

SHA512Hasher::SHA512Hasher(ID id) noexcept : id_(id) { reset(); }

SHA512Hasher::SHA512Hasher(const SHA512Hasher& src) noexcept : id_(src.id_) {
  ::memcpy(&raw_, &src.raw_, sizeof(raw_));
}

uint16_t SHA512Hasher::output_size() const noexcept {
  switch (id_) {
    case ID::sha512_224:
      return SHA512_224_SUMSIZE;

    case ID::sha512_256:
      return SHA512_256_SUMSIZE;

    case ID::sha384:
      return SHA384_SUMSIZE;

    default:
      return SHA512_SUMSIZE;
  }
}

std::unique_ptr<Hasher> SHA512Hasher::copy() const {
  return base::backport::make_unique<SHA512Hasher>(*this);
}

void SHA512Hasher::reset() {
  ::bzero(&raw_, sizeof(raw_));
  const uint64_t* h;
  switch (id_) {
    case ID::sha512_224:
      h = SHA512_224_H;
      break;

    case ID::sha512_256:
      h = SHA512_256_H;
      break;

    case ID::sha384:
      h = SHA384_H;
      break;

    default:
      h = SHA512_H;
  }
  raw_.h[0] = h[0];
  raw_.h[1] = h[1];
  raw_.h[2] = h[2];
  raw_.h[3] = h[3];
  raw_.h[4] = h[4];
  raw_.h[5] = h[5];
  raw_.h[6] = h[6];
  raw_.h[7] = h[7];
}

void SHA512Hasher::write(base::Bytes in) {
  CHECK(!raw_.finalized) << ": hash is finalized";

  const auto* ptr = in.data();
  auto len = in.size();

  raw_.len += len;
  unsigned int nx = raw_.nx;
  if (nx) {
    auto n = std::min(SHA512_BLOCKSIZE - nx, len);
    ::memcpy(raw_.x + nx, ptr, n);
    nx += n;
    ptr += n;
    len -= n;
    if (nx == SHA512_BLOCKSIZE) {
      block(raw_.x, SHA512_BLOCKSIZE);
      nx = 0;
    }
    raw_.nx = nx;
  }
  if (len >= SHA512_BLOCKSIZE) {
    uint64_t n = len & ~(SHA512_BLOCKSIZE - 1);
    block(ptr, n);
    ptr += n;
    len -= n;
  }
  if (len) {
    ::memcpy(raw_.x, ptr, len);
    raw_.nx = len;
  }
}

void SHA512Hasher::finalize() {
  CHECK(!raw_.finalized) << ": hash is finalized";

  uint8_t tmp[SHA512_BLOCKSIZE];
  ::bzero(tmp, sizeof(tmp));
  tmp[0] = 0x80;

  uint64_t len = raw_.len;
  uint8_t n = (len & (SHA512_BLOCKSIZE - 1));
  if (n < 112)
    write(base::Bytes(tmp, 112 - n));
  else
    write(base::Bytes(tmp, SHA512_BLOCKSIZE + 112 - n));

  uint64_t hi = (len >> 61);
  uint64_t lo = (len << 3);
  WBE64(tmp, 0, hi);
  WBE64(tmp, 1, lo);
  write(base::Bytes(tmp, 16));

  DCHECK_EQ(raw_.nx, 0U);
  raw_.finalized = true;
}

void SHA512Hasher::sum(base::MutableBytes out) {
  CHECK(raw_.finalized) << ": hash is not finalized";
  CHECK_GE(out.size(), output_size());
  WBE64(out.data(), 0, raw_.h[0]);
  WBE64(out.data(), 1, raw_.h[1]);
  WBE64(out.data(), 2, raw_.h[2]);
  if (id_ == ID::sha512_224) {
    out[24] = (raw_.h[3] >> 56);
    out[25] = (raw_.h[3] >> 48);
    out[26] = (raw_.h[3] >> 40);
    out[27] = (raw_.h[3] >> 32);
    return;
  }
  WBE64(out.data(), 3, raw_.h[3]);
  if (id_ == ID::sha512_256) return;
  WBE64(out.data(), 4, raw_.h[4]);
  WBE64(out.data(), 5, raw_.h[5]);
  if (id_ == ID::sha384) return;
  WBE64(out.data(), 6, raw_.h[6]);
  WBE64(out.data(), 7, raw_.h[7]);
}

void SHA512Hasher::block(const uint8_t* ptr, uint64_t len) {
  uint64_t w[80];

  uint64_t h0 = raw_.h[0];
  uint64_t h1 = raw_.h[1];
  uint64_t h2 = raw_.h[2];
  uint64_t h3 = raw_.h[3];
  uint64_t h4 = raw_.h[4];
  uint64_t h5 = raw_.h[5];
  uint64_t h6 = raw_.h[6];
  uint64_t h7 = raw_.h[7];

  uint64_t a, b, c, d, e, f, g, h;
  uint64_t s0, s1, ch, maj, temp1, temp2;
  unsigned int i;

  while (len >= SHA512_BLOCKSIZE) {
    i = 0;
    while (i < 16) {
      w[i] = RBE64(ptr, i);
      ++i;
    }
    while (i < 80) {
      s0 = w[i - 15];
      s1 = w[i - 2];
      s0 = ROR64(s0, 1) ^ ROR64(s0, 8) ^ (s0 >> 7);
      s1 = ROR64(s1, 19) ^ ROR64(s1, 61) ^ (s1 >> 6);
      w[i] = w[i - 16] + w[i - 7] + s0 + s1;
      ++i;
    }

    a = h0;
    b = h1;
    c = h2;
    d = h3;
    e = h4;
    f = h5;
    g = h6;
    h = h7;

    for (i = 0; i < 80; ++i) {
      s1 = ROR64(e, 14) ^ ROR64(e, 18) ^ ROR64(e, 41);
      ch = (e & f) ^ ((~e) & g);
      temp1 = h + s1 + ch + K[i] + w[i];
      s0 = ROR64(a, 28) ^ ROR64(a, 34) ^ ROR64(a, 39);
      maj = (a & b) ^ (a & c) ^ (b & c);
      temp2 = s0 + maj;
      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
    h5 += f;
    h6 += g;
    h7 += h;

    ptr += SHA512_BLOCKSIZE;
    len -= SHA512_BLOCKSIZE;
  }
  DCHECK_EQ(len, 0U);

  raw_.h[0] = h0;
  raw_.h[1] = h1;
  raw_.h[2] = h2;
  raw_.h[3] = h3;
  raw_.h[4] = h4;
  raw_.h[5] = h5;
  raw_.h[6] = h6;
  raw_.h[7] = h7;
}
}  // inline namespace implementation

std::unique_ptr<Hasher> new_sha384() {
  return base::backport::make_unique<SHA512Hasher>(SHA512Hasher::ID::sha384);
}

std::unique_ptr<Hasher> new_sha512() {
  return base::backport::make_unique<SHA512Hasher>(SHA512Hasher::ID::sha512);
}

std::unique_ptr<Hasher> new_sha512_224() {
  return base::backport::make_unique<SHA512Hasher>(
      SHA512Hasher::ID::sha512_224);
}

std::unique_ptr<Hasher> new_sha512_256() {
  return base::backport::make_unique<SHA512Hasher>(
      SHA512Hasher::ID::sha512_256);
}
}  // namespace hash
}  // namespace crypto

static const crypto::Hash SHA384 = {
    crypto::hash::SHA512_BLOCKSIZE,  // block_size
    crypto::hash::SHA384_SUMSIZE,    // output_size
    crypto::Security::secure,        // security
    0,                               // flags
    "SHA-384",                       // name
    crypto::hash::new_sha384,        // newfn
    nullptr,                         // varfn
};

static const crypto::Hash SHA512 = {
    crypto::hash::SHA512_BLOCKSIZE,  // block_size
    crypto::hash::SHA512_SUMSIZE,    // output_size
    crypto::Security::secure,        // security
    0,                               // flags
    "SHA-512",                       // name
    crypto::hash::new_sha512,        // newfn
    nullptr,                         // varfn
};

static const crypto::Hash SHA512_224 = {
    crypto::hash::SHA512_BLOCKSIZE,    // block_size
    crypto::hash::SHA512_224_SUMSIZE,  // output_size
    crypto::Security::secure,          // security
    0,                                 // flags
    "SHA-512/224",                     // name
    crypto::hash::new_sha512_224,      // newfn
    nullptr,                           // varfn
};

static const crypto::Hash SHA512_256 = {
    crypto::hash::SHA512_BLOCKSIZE,    // block_size
    crypto::hash::SHA512_256_SUMSIZE,  // output_size
    crypto::Security::secure,          // security
    0,                                 // flags
    "SHA-512/256",                     // name
    crypto::hash::new_sha512_256,      // newfn
    nullptr,                           // varfn
};

static void init() __attribute__((constructor));
static void init() {
  crypto::register_hash(&SHA384);
  crypto::register_hash(&SHA512);
  crypto::register_hash(&SHA512_224);
  crypto::register_hash(&SHA512_256);
}
