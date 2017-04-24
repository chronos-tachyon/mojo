// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/md5.h"

#include "base/logging.h"
#include "crypto/primitives.h"

using crypto::primitives::ROL32;
using crypto::primitives::RLE32;
using crypto::primitives::WLE32;
using crypto::primitives::WLE64;

static const uint32_t H[4] = {
    0x67452301U, 0xefcdab89U, 0x98badcfeU, 0x10325476U,
};

static const unsigned int S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

static const uint32_t K[64] = {
    0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU, 0xf57c0fafU,
    0x4787c62aU, 0xa8304613U, 0xfd469501U, 0x698098d8U, 0x8b44f7afU,
    0xffff5bb1U, 0x895cd7beU, 0x6b901122U, 0xfd987193U, 0xa679438eU,
    0x49b40821U, 0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU,
    0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U, 0x21e1cde6U,
    0xc33707d6U, 0xf4d50d87U, 0x455a14edU, 0xa9e3e905U, 0xfcefa3f8U,
    0x676f02d9U, 0x8d2a4c8aU, 0xfffa3942U, 0x8771f681U, 0x6d9d6122U,
    0xfde5380cU, 0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
    0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U, 0xd9d4d039U,
    0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U, 0xf4292244U, 0x432aff97U,
    0xab9423a7U, 0xfc93a039U, 0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU,
    0x85845dd1U, 0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U,
    0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U,
};

static inline uint32_t F0(uint32_t p, uint32_t q, uint32_t r) {
  // return (p & q) | ((~p) & r);
  return ((q ^ r) & p) ^ r;
}

static inline uint32_t F1(uint32_t p, uint32_t q, uint32_t r) {
  return F0(r, p, q);
}

static inline uint32_t F2(uint32_t p, uint32_t q, uint32_t r) {
  return p ^ q ^ r;
}

static inline uint32_t F3(uint32_t p, uint32_t q, uint32_t r) {
  return q ^ (p | (~r));
}

namespace crypto {
namespace hash {
inline namespace implementation {
class MD5Hasher : public Hasher {
 public:
  struct Raw {
    uint8_t x[MD5_BLOCKSIZE];
    uint32_t h[4];
    uint64_t len;
    uint8_t nx;
    bool finalized;
  };

  MD5Hasher() noexcept;
  MD5Hasher(const MD5Hasher& src) noexcept;

  uint16_t block_size() const noexcept override { return MD5_BLOCKSIZE; }
  uint16_t output_size() const noexcept override { return MD5_SUMSIZE; }
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

MD5Hasher::MD5Hasher() noexcept { reset(); }

MD5Hasher::MD5Hasher(const MD5Hasher& src) noexcept {
  ::memcpy(&raw_, &src.raw_, sizeof(raw_));
}

std::unique_ptr<Hasher> MD5Hasher::copy() const {
  return base::backport::make_unique<MD5Hasher>(*this);
}

void MD5Hasher::reset() {
  ::bzero(&raw_, sizeof(raw_));
  raw_.h[0] = H[0];
  raw_.h[1] = H[1];
  raw_.h[2] = H[2];
  raw_.h[3] = H[3];
}

void MD5Hasher::write(base::Bytes in) {
  CHECK(!raw_.finalized) << ": hash is finalized";

  const auto* ptr = in.data();
  auto len = in.size();

  raw_.len += len;
  unsigned int nx = raw_.nx;
  if (nx) {
    auto n = std::min(MD5_BLOCKSIZE - nx, len);
    ::memcpy(raw_.x + nx, ptr, n);
    nx += n;
    ptr += n;
    len -= n;
    if (nx == MD5_BLOCKSIZE) {
      block(raw_.x, MD5_BLOCKSIZE);
      nx = 0;
    }
    raw_.nx = nx;
  }
  if (len >= MD5_BLOCKSIZE) {
    uint64_t n = len & ~(MD5_BLOCKSIZE - 1);
    block(ptr, n);
    ptr += n;
    len -= n;
  }
  if (len) {
    ::memcpy(raw_.x, ptr, len);
    raw_.nx = len;
  }
}

void MD5Hasher::finalize() {
  CHECK(!raw_.finalized) << ": hash is finalized";

  uint8_t tmp[MD5_BLOCKSIZE];
  ::bzero(tmp, sizeof(tmp));
  tmp[0] = 0x80;

  uint64_t len = raw_.len;
  uint8_t n = (len & (MD5_BLOCKSIZE - 1));
  if (n < 56)
    write(base::Bytes(tmp, 56 - n));
  else
    write(base::Bytes(tmp, MD5_BLOCKSIZE + 56 - n));

  len <<= 3;
  WLE64(tmp, 0, len);
  write(base::Bytes(tmp, 8));

  DCHECK_EQ(raw_.nx, 0U);
  raw_.finalized = true;
}

void MD5Hasher::sum(base::MutableBytes out) {
  CHECK(raw_.finalized) << ": hash is not finalized";
  CHECK_GE(out.size(), MD5_SUMSIZE);
  WLE32(out.data(), 0, raw_.h[0]);
  WLE32(out.data(), 1, raw_.h[1]);
  WLE32(out.data(), 2, raw_.h[2]);
  WLE32(out.data(), 3, raw_.h[3]);
}

void MD5Hasher::block(const uint8_t* ptr, uint64_t len) {
  uint32_t m[16];

  uint32_t h0 = raw_.h[0];
  uint32_t h1 = raw_.h[1];
  uint32_t h2 = raw_.h[2];
  uint32_t h3 = raw_.h[3];

  uint32_t a, b, c, d, f, temp;
  unsigned int i, g;

  while (len >= MD5_BLOCKSIZE) {
    i = 0;
    while (i < 16) {
      m[i] = RLE32(ptr, i);
      ++i;
    }

    a = h0;
    b = h1;
    c = h2;
    d = h3;

    for (i = 0; i < 16; ++i) {
      f = F0(b, c, d);
      g = i;
      temp = d;
      d = c;
      c = b;
      b = b + ROL32(a + f + K[i] + m[g], S[i]);
      a = temp;
    }
    for (i = 16; i < 32; ++i) {
      f = F1(b, c, d);
      g = ((i * 5) + 1) & 15;
      temp = d;
      d = c;
      c = b;
      b = b + ROL32(a + f + K[i] + m[g], S[i]);
      a = temp;
    }
    for (i = 32; i < 48; ++i) {
      f = F2(b, c, d);
      g = ((i * 3) + 5) & 15;
      temp = d;
      d = c;
      c = b;
      b = b + ROL32(a + f + K[i] + m[g], S[i]);
      a = temp;
    }
    for (i = 48; i < 64; ++i) {
      f = F3(b, c, d);
      g = (i * 7) & 15;
      temp = d;
      d = c;
      c = b;
      b = b + ROL32(a + f + K[i] + m[g], S[i]);
      a = temp;
    }

    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;

    ptr += MD5_BLOCKSIZE;
    len -= MD5_BLOCKSIZE;
  }
  DCHECK_EQ(len, 0U);

  raw_.h[0] = h0;
  raw_.h[1] = h1;
  raw_.h[2] = h2;
  raw_.h[3] = h3;
}
}  // inline namespace implementation

std::unique_ptr<Hasher> new_md5() {
  return base::backport::make_unique<MD5Hasher>();
}
}  // namespace hash
}  // namespace crypto

static const crypto::Hash MD5 = {
    crypto::hash::MD5_BLOCKSIZE,  // block_size
    crypto::hash::MD5_SUMSIZE,    // output_size
    crypto::Security::broken,     // security
    0,                            // flags
    "MD5",                        // name
    crypto::hash::new_md5,        // newfn
    nullptr,                      // varfn
};

static void init() __attribute__((constructor));
static void init() { crypto::register_hash(&MD5); }
