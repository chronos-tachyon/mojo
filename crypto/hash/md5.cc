// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/hash.h"

#include "base/logging.h"

static constexpr bool OPT =
#if defined(__i386__) || defined(__x86_64__)
    true;
#else
    false;
#endif

static constexpr std::size_t BLOCKSIZE = 64;
static constexpr std::size_t SUMSIZE = 16;

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

static inline __attribute__((always_inline)) uint32_t L(uint32_t x,
                                                        unsigned int c) {
  return (x << c) | (x >> (32 - c));
}

static inline __attribute__((always_inline)) uint32_t F0(uint32_t p, uint32_t q,
                                                         uint32_t r) {
  // return (p & q) | ((~p) & r);
  return ((q ^ r) & p) ^ r;
}

static inline __attribute__((always_inline)) uint32_t F1(uint32_t p, uint32_t q,
                                                         uint32_t r) {
  return F0(r, p, q);
}

static inline __attribute__((always_inline)) uint32_t F2(uint32_t p, uint32_t q,
                                                         uint32_t r) {
  return p ^ q ^ r;
}

static inline __attribute__((always_inline)) uint32_t F3(uint32_t p, uint32_t q,
                                                         uint32_t r) {
  return q ^ (p | (~r));
}

static inline __attribute__((always_inline)) uint32_t X(const uint8_t* ptr,
                                                        unsigned int index) {
  if (OPT) {
    const uint32_t* ptr32 = reinterpret_cast<const uint32_t*>(ptr);
    return ptr32[index];
  } else {
    unsigned int byte0 = ptr[(index * 4) + 0];
    unsigned int byte1 = ptr[(index * 4) + 1];
    unsigned int byte2 = ptr[(index * 4) + 2];
    unsigned int byte3 = ptr[(index * 4) + 3];
    return byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24);
  }
}

static inline __attribute__((always_inline)) void Y(uint8_t* out,
                                                    uint32_t value,
                                                    unsigned int index) {
  if (OPT) {
    uint32_t* out32 = reinterpret_cast<uint32_t*>(out);
    out32[index] = value;
  } else {
    out[(index * 4) + 0] = (value & 0xffU);
    out[(index * 4) + 1] = ((value >> 8) & 0xffU);
    out[(index * 4) + 2] = ((value >> 16) & 0xffU);
    out[(index * 4) + 3] = ((value >> 24) & 0xffU);
  }
}

static inline __attribute__((always_inline)) void YY(uint8_t* out,
                                                     uint64_t value,
                                                     unsigned int index) {
  if (OPT) {
    uint64_t* out64 = reinterpret_cast<uint64_t*>(out);
    out64[index] = value;
  } else {
    out[(index * 8) + 0] = (value & 0xffU);
    out[(index * 8) + 1] = ((value >> 8) & 0xffU);
    out[(index * 8) + 2] = ((value >> 16) & 0xffU);
    out[(index * 8) + 3] = ((value >> 24) & 0xffU);
    out[(index * 8) + 4] = ((value >> 32) & 0xffU);
    out[(index * 8) + 5] = ((value >> 40) & 0xffU);
    out[(index * 8) + 6] = ((value >> 48) & 0xffU);
    out[(index * 8) + 7] = ((value >> 56) & 0xffU);
  }
}

namespace crypto {
namespace hash {
inline namespace implementation {
class MD5State : public State {
 public:
  struct Raw {
    uint8_t x[BLOCKSIZE];
    uint32_t h[4];
    uint64_t len;
    uint8_t nx;
    bool finalized;
  };

  MD5State() noexcept;
  MD5State(const MD5State& src) noexcept;

  const Algorithm& algorithm() const noexcept override { return MD5; }
  std::unique_ptr<State> copy() const override;
  void write(const uint8_t* ptr, std::size_t len) override;
  void finalize() override;
  void sum(uint8_t* ptr, std::size_t len) override;
  void reset() override;

 private:
  void block(const uint8_t* ptr, uint64_t len);

  Raw raw_;
};

MD5State::MD5State() noexcept { reset(); }

MD5State::MD5State(const MD5State& src) noexcept {
  ::memcpy(&raw_, &src.raw_, sizeof(raw_));
}

std::unique_ptr<State> MD5State::copy() const {
  return base::backport::make_unique<MD5State>(*this);
}

void MD5State::write(const uint8_t* ptr, std::size_t len) {
  CHECK(!raw_.finalized) << ": hash is finalized";

  raw_.len += len;
  unsigned int nx = raw_.nx;
  if (nx) {
    auto n = std::min(BLOCKSIZE - nx, len);
    ::memcpy(raw_.x + nx, ptr, n);
    nx += n;
    ptr += n;
    len -= n;
    if (nx == BLOCKSIZE) {
      block(raw_.x, BLOCKSIZE);
      nx = 0;
    }
    raw_.nx = nx;
  }
  if (len >= BLOCKSIZE) {
    uint64_t n = len & ~(BLOCKSIZE - 1);
    block(ptr, n);
    ptr += n;
    len -= n;
  }
  if (len) {
    ::memcpy(raw_.x, ptr, len);
    raw_.nx = len;
  }
}

void MD5State::finalize() {
  CHECK(!raw_.finalized) << ": hash is finalized";

  uint8_t tmp[BLOCKSIZE];
  ::bzero(tmp, sizeof(tmp));
  tmp[0] = 0x80;

  uint64_t len = raw_.len;
  uint8_t n = (len & (BLOCKSIZE - 1));
  if (n < 56)
    write(tmp, 56 - n);
  else
    write(tmp, BLOCKSIZE + 56 - n);

  len <<= 3;
  YY(tmp, len, 0);
  write(tmp, 8);

  DCHECK_EQ(raw_.nx, 0U);
  raw_.finalized = true;
}

void MD5State::sum(uint8_t* ptr, std::size_t len) {
  CHECK(raw_.finalized) << ": hash is not finalized";
  CHECK_EQ(len, SUMSIZE);
  Y(ptr, raw_.h[0], 0);
  Y(ptr, raw_.h[1], 1);
  Y(ptr, raw_.h[2], 2);
  Y(ptr, raw_.h[3], 3);
}

void MD5State::reset() {
  ::bzero(&raw_, sizeof(raw_));
  raw_.h[0] = H[0];
  raw_.h[1] = H[1];
  raw_.h[2] = H[2];
  raw_.h[3] = H[3];
}

void MD5State::block(const uint8_t* ptr, uint64_t len) {
  uint32_t m[16];

  uint32_t h0 = raw_.h[0];
  uint32_t h1 = raw_.h[1];
  uint32_t h2 = raw_.h[2];
  uint32_t h3 = raw_.h[3];

  uint32_t a, b, c, d, f, temp;
  unsigned int i, g;

  while (len >= BLOCKSIZE) {
    i = 0;
    while (i < 16) {
      m[i] = X(ptr, i);
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
      b = b + L(a + f + K[i] + m[g], S[i]);
      a = temp;
    }
    for (i = 16; i < 32; ++i) {
      f = F1(b, c, d);
      g = ((i * 5) + 1) & 15;
      temp = d;
      d = c;
      c = b;
      b = b + L(a + f + K[i] + m[g], S[i]);
      a = temp;
    }
    for (i = 32; i < 48; ++i) {
      f = F2(b, c, d);
      g = ((i * 3) + 5) & 15;
      temp = d;
      d = c;
      c = b;
      b = b + L(a + f + K[i] + m[g], S[i]);
      a = temp;
    }
    for (i = 48; i < 64; ++i) {
      f = F3(b, c, d);
      g = (i * 7) & 15;
      temp = d;
      d = c;
      c = b;
      b = b + L(a + f + K[i] + m[g], S[i]);
      a = temp;
    }

    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;

    len -= BLOCKSIZE;
  }
  DCHECK_EQ(len, 0U);

  raw_.h[0] = h0;
  raw_.h[1] = h1;
  raw_.h[2] = h2;
  raw_.h[3] = h3;
}

std::unique_ptr<State> new_md5() {
  return base::backport::make_unique<MD5State>();
}
}  // inline namespace implementation

const Algorithm MD5 = {
    ID::md5, "MD5", BLOCKSIZE, SUMSIZE, Security::broken, new_md5, nullptr,
};

}  // namespace hash
}  // namespace crypto
