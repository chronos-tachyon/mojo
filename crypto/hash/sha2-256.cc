// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/sha2.h"

#include "base/logging.h"
#include "crypto/primitives.h"

using crypto::primitives::ROR32;
using crypto::primitives::RBE32;
using crypto::primitives::WBE32;
using crypto::primitives::WBE64;

static const uint32_t SHA224_H[8] = {
    0xc1059ed8U, 0x367cd507U, 0x3070dd17U, 0xf70e5939U,
    0xffc00b31U, 0x68581511U, 0x64f98fa7U, 0xbefa4fa4U,
};

static const uint32_t SHA256_H[8] = {
    0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
    0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
};

static const uint32_t K[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
    0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
    0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
    0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
    0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
    0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
    0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
    0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

namespace crypto {
namespace hash {

inline namespace implementation {
class SHA256Hasher : public Hasher {
 public:
  struct Raw {
    uint8_t x[SHA256_BLOCKSIZE];
    uint32_t h[8];
    uint64_t len;
    uint8_t nx;
    bool finalized;
  };

  explicit SHA256Hasher(bool narrow) noexcept;
  SHA256Hasher(const SHA256Hasher& src) noexcept;

  uint16_t block_size() const noexcept override { return SHA256_BLOCKSIZE; }
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
  bool narrow_;
};

SHA256Hasher::SHA256Hasher(bool narrow) noexcept : narrow_(narrow) { reset(); }

SHA256Hasher::SHA256Hasher(const SHA256Hasher& src) noexcept
    : narrow_(src.narrow_) {
  ::memcpy(&raw_, &src.raw_, sizeof(raw_));
}

uint16_t SHA256Hasher::output_size() const noexcept {
  return (narrow_ ? SHA224_SUMSIZE : SHA256_SUMSIZE);
}

std::unique_ptr<Hasher> SHA256Hasher::copy() const {
  return base::backport::make_unique<SHA256Hasher>(*this);
}

void SHA256Hasher::reset() {
  ::bzero(&raw_, sizeof(raw_));
  const uint32_t* h = (narrow_ ? SHA224_H : SHA256_H);
  raw_.h[0] = h[0];
  raw_.h[1] = h[1];
  raw_.h[2] = h[2];
  raw_.h[3] = h[3];
  raw_.h[4] = h[4];
  raw_.h[5] = h[5];
  raw_.h[6] = h[6];
  raw_.h[7] = h[7];
}

void SHA256Hasher::write(base::Bytes in) {
  CHECK(!raw_.finalized) << ": hash is finalized";

  const auto* ptr = in.data();
  auto len = in.size();

  raw_.len += len;
  unsigned int nx = raw_.nx;
  if (nx) {
    auto n = std::min(SHA256_BLOCKSIZE - nx, len);
    ::memcpy(raw_.x + nx, ptr, n);
    nx += n;
    ptr += n;
    len -= n;
    if (nx == SHA256_BLOCKSIZE) {
      block(raw_.x, SHA256_BLOCKSIZE);
      nx = 0;
    }
    raw_.nx = nx;
  }
  if (len >= SHA256_BLOCKSIZE) {
    uint64_t n = len & ~(SHA256_BLOCKSIZE - 1);
    block(ptr, n);
    ptr += n;
    len -= n;
  }
  if (len) {
    ::memcpy(raw_.x, ptr, len);
    raw_.nx = len;
  }
}

void SHA256Hasher::finalize() {
  CHECK(!raw_.finalized) << ": hash is finalized";

  uint8_t tmp[SHA256_BLOCKSIZE];
  ::bzero(tmp, sizeof(tmp));
  tmp[0] = 0x80;

  uint64_t len = raw_.len;
  uint8_t n = (len & (SHA256_BLOCKSIZE - 1));
  if (n < 56)
    write(base::Bytes(tmp, 56 - n));
  else
    write(base::Bytes(tmp, SHA256_BLOCKSIZE + 56 - n));

  len <<= 3;
  WBE64(tmp, 0, len);
  write(base::Bytes(tmp, 8));

  DCHECK_EQ(raw_.nx, 0U);
  raw_.finalized = true;
}

void SHA256Hasher::sum(base::MutableBytes out) {
  CHECK(raw_.finalized) << ": hash is not finalized";
  CHECK_GE(out.size(), output_size());
  WBE32(out.data(), 0, raw_.h[0]);
  WBE32(out.data(), 1, raw_.h[1]);
  WBE32(out.data(), 2, raw_.h[2]);
  WBE32(out.data(), 3, raw_.h[3]);
  WBE32(out.data(), 4, raw_.h[4]);
  WBE32(out.data(), 5, raw_.h[5]);
  WBE32(out.data(), 6, raw_.h[6]);
  if (narrow_) return;
  WBE32(out.data(), 7, raw_.h[7]);
}

void SHA256Hasher::block(const uint8_t* ptr, uint64_t len) {
  uint32_t w[64];

  uint32_t h0 = raw_.h[0];
  uint32_t h1 = raw_.h[1];
  uint32_t h2 = raw_.h[2];
  uint32_t h3 = raw_.h[3];
  uint32_t h4 = raw_.h[4];
  uint32_t h5 = raw_.h[5];
  uint32_t h6 = raw_.h[6];
  uint32_t h7 = raw_.h[7];

  uint32_t a, b, c, d, e, f, g, h;
  uint32_t s0, s1, ch, maj, temp1, temp2;
  unsigned int i;

  while (len >= SHA256_BLOCKSIZE) {
    i = 0;
    while (i < 16) {
      w[i] = RBE32(ptr, i);
      ++i;
    }
    while (i < 64) {
      s0 = w[i - 15];
      s1 = w[i - 2];
      s0 = ROR32(s0, 7) ^ ROR32(s0, 18) ^ (s0 >> 3);
      s1 = ROR32(s1, 17) ^ ROR32(s1, 19) ^ (s1 >> 10);
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

    for (i = 0; i < 64; ++i) {
      s1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
      ch = (e & f) ^ ((~e) & g);
      temp1 = h + s1 + ch + K[i] + w[i];
      s0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
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

    ptr += SHA256_BLOCKSIZE;
    len -= SHA256_BLOCKSIZE;
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

std::unique_ptr<Hasher> new_sha224() {
  return base::backport::make_unique<SHA256Hasher>(true);
}

std::unique_ptr<Hasher> new_sha256() {
  return base::backport::make_unique<SHA256Hasher>(false);
}
}  // namespace hash
}  // namespace crypto

static const crypto::Hash SHA224 = {
    crypto::hash::SHA256_BLOCKSIZE,  // block_size
    crypto::hash::SHA224_SUMSIZE,    // output_size
    crypto::Security::secure,        // security
    0,                               // flags
    "SHA-224",                       // name
    crypto::hash::new_sha224,        // newfn
    nullptr,                         // varfn
};

static const crypto::Hash SHA256 = {
    crypto::hash::SHA256_BLOCKSIZE,  // block_size
    crypto::hash::SHA256_SUMSIZE,    // output_size
    crypto::Security::secure,        // security
    0,                               // flags
    "SHA-256",                       // name
    crypto::hash::new_sha256,        // newfn
    nullptr,                         // varfn
};

static void init() __attribute__((constructor));
static void init() {
  crypto::register_hash(&SHA224);
  crypto::register_hash(&SHA256);
}
