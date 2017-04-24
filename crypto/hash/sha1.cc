// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/sha1.h"

#include "base/logging.h"
#include "crypto/primitives.h"

using crypto::primitives::ROL32;
using crypto::primitives::RBE32;
using crypto::primitives::WBE32;
using crypto::primitives::WBE64;

static const uint32_t H[5] = {
    0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U,
};

static inline uint32_t F0(uint32_t p, uint32_t q, uint32_t r) {
  // return (p & q) | ((~p) & r);
  return ((q ^ r) & p) ^ r;
}

static inline uint32_t F1(uint32_t p, uint32_t q, uint32_t r) {
  return p ^ q ^ r;
}

static inline uint32_t F2(uint32_t p, uint32_t q, uint32_t r) {
  return (p & q) | (p & r) | (q & r);
}

namespace crypto {
namespace hash {
inline namespace implementation {
class SHA1Hasher : public Hasher {
 public:
  struct Raw {
    uint8_t x[SHA1_BLOCKSIZE];
    uint32_t h[5];
    uint64_t len;
    uint8_t nx;
    bool finalized;
  };

  SHA1Hasher() noexcept;
  SHA1Hasher(const SHA1Hasher& src) noexcept;

  uint16_t block_size() const noexcept override { return SHA1_BLOCKSIZE; }
  uint16_t output_size() const noexcept override { return SHA1_SUMSIZE; }
  bool is_sponge() const noexcept override { return false; }

  std::unique_ptr<Hasher> copy() const override;
  void reset() override;
  void write(base::Bytes in) override;
  void finalize() override;
  void sum(base::MutableBytes out) override;

 private:
  void block(const uint8_t* ptr, uint64_t len);

  Raw raw_;
};

SHA1Hasher::SHA1Hasher() noexcept { reset(); }

SHA1Hasher::SHA1Hasher(const SHA1Hasher& src) noexcept {
  ::memcpy(&raw_, &src.raw_, sizeof(raw_));
}

std::unique_ptr<Hasher> SHA1Hasher::copy() const {
  return base::backport::make_unique<SHA1Hasher>(*this);
}

void SHA1Hasher::reset() {
  ::bzero(&raw_, sizeof(raw_));
  raw_.h[0] = H[0];
  raw_.h[1] = H[1];
  raw_.h[2] = H[2];
  raw_.h[3] = H[3];
  raw_.h[4] = H[4];
}

void SHA1Hasher::write(base::Bytes in) {
  CHECK(!raw_.finalized) << ": hash is finalized";

  const auto* ptr = in.data();
  auto len = in.size();

  raw_.len += len;
  unsigned int nx = raw_.nx;
  if (nx) {
    auto n = std::min(SHA1_BLOCKSIZE - nx, len);
    ::memcpy(raw_.x + nx, ptr, n);
    nx += n;
    ptr += n;
    len -= n;
    if (nx == SHA1_BLOCKSIZE) {
      block(raw_.x, SHA1_BLOCKSIZE);
      nx = 0;
    }
    raw_.nx = nx;
  }
  if (len >= SHA1_BLOCKSIZE) {
    uint64_t n = len & ~(SHA1_BLOCKSIZE - 1);
    block(ptr, n);
    ptr += n;
    len -= n;
  }
  if (len) {
    ::memcpy(raw_.x, ptr, len);
    raw_.nx = len;
  }
}

void SHA1Hasher::finalize() {
  CHECK(!raw_.finalized) << ": hash is finalized";

  uint8_t tmp[SHA1_BLOCKSIZE];
  ::bzero(tmp, sizeof(tmp));
  tmp[0] = 0x80;

  uint64_t len = raw_.len;
  uint8_t n = (len & (SHA1_BLOCKSIZE - 1));
  if (n < 56)
    write(base::Bytes(tmp, 56 - n));
  else
    write(base::Bytes(tmp, SHA1_BLOCKSIZE + 56 - n));

  len <<= 3;
  WBE64(tmp, 0, len);
  write(base::Bytes(tmp, 8));

  DCHECK_EQ(raw_.nx, 0U);
  raw_.finalized = true;
}

void SHA1Hasher::sum(base::MutableBytes out) {
  CHECK(raw_.finalized) << ": hash is not finalized";
  CHECK_GE(out.size(), SHA1_SUMSIZE);
  WBE32(out.data(), 0, raw_.h[0]);
  WBE32(out.data(), 1, raw_.h[1]);
  WBE32(out.data(), 2, raw_.h[2]);
  WBE32(out.data(), 3, raw_.h[3]);
  WBE32(out.data(), 4, raw_.h[4]);
}

void SHA1Hasher::block(const uint8_t* ptr, uint64_t len) {
  uint32_t w[80];

  uint32_t h0 = raw_.h[0];
  uint32_t h1 = raw_.h[1];
  uint32_t h2 = raw_.h[2];
  uint32_t h3 = raw_.h[3];
  uint32_t h4 = raw_.h[4];

  uint32_t a, b, c, d, e, f, k, temp;
  unsigned int i;

  while (len >= SHA1_BLOCKSIZE) {
    i = 0;
    while (i < 16) {
      w[i] = RBE32(ptr, i);
      ++i;
    }
    while (i < 80) {
      w[i] = ROL32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
      ++i;
    }

    a = h0;
    b = h1;
    c = h2;
    d = h3;
    e = h4;

    for (i = 0; i < 20; ++i) {
      f = F0(b, c, d);
      k = 0x5a827999U;
      temp = ROL32(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = ROL32(b, 30);
      b = a;
      a = temp;
    }
    for (i = 20; i < 40; ++i) {
      f = F1(b, c, d);
      k = 0x6ed9eba1U;
      temp = ROL32(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = ROL32(b, 30);
      b = a;
      a = temp;
    }
    for (i = 40; i < 60; ++i) {
      f = F2(b, c, d);
      k = 0x8f1bbcdcU;
      temp = ROL32(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = ROL32(b, 30);
      b = a;
      a = temp;
    }
    for (i = 60; i < 80; ++i) {
      f = F1(b, c, d);
      k = 0xca62c1d6U;
      temp = ROL32(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = ROL32(b, 30);
      b = a;
      a = temp;
    }

    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;

    ptr += SHA1_BLOCKSIZE;
    len -= SHA1_BLOCKSIZE;
  }
  DCHECK_EQ(len, 0U);

  raw_.h[0] = h0;
  raw_.h[1] = h1;
  raw_.h[2] = h2;
  raw_.h[3] = h3;
  raw_.h[4] = h4;
}
}  // inline namespace implementation

std::unique_ptr<Hasher> new_sha1() {
  return base::backport::make_unique<SHA1Hasher>();
}
}  // namespace hash
}  // namespace crypto

static const crypto::Hash SHA1 = {
    crypto::hash::SHA1_BLOCKSIZE,  // block_size
    crypto::hash::SHA1_SUMSIZE,    // output_size
    crypto::Security::broken,      // security
    0,                             // flags
    "SHA-1",                       // name
    crypto::hash::new_sha1,        // newfn
    nullptr,                       // varfn
};

static void init() __attribute__((constructor));
static void init() { crypto::register_hash(&SHA1); }
