// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/hash.h"

#include "base/logging.h"

static constexpr std::size_t BLOCKSIZE = 64;
static constexpr std::size_t SUMSIZE = 20;

static const uint32_t H[5] = {
    0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U,
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
  return p ^ q ^ r;
}

static inline __attribute__((always_inline)) uint32_t F2(uint32_t p, uint32_t q,
                                                         uint32_t r) {
  return (p & q) | (p & r) | (q & r);
}

static inline __attribute__((always_inline)) uint32_t X(const uint8_t* ptr,
                                                        unsigned int index) {
  unsigned int byte0 = ptr[(index * 4) + 0];
  unsigned int byte1 = ptr[(index * 4) + 1];
  unsigned int byte2 = ptr[(index * 4) + 2];
  unsigned int byte3 = ptr[(index * 4) + 3];
  return (byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3;
}

static inline __attribute__((always_inline)) void Y(uint8_t* out,
                                                    uint32_t value,
                                                    unsigned int index) {
  out[(index * 4) + 0] = ((value >> 24) & 0xffU);
  out[(index * 4) + 1] = ((value >> 16) & 0xffU);
  out[(index * 4) + 2] = ((value >> 8) & 0xffU);
  out[(index * 4) + 3] = (value & 0xffU);
}

static inline __attribute__((always_inline)) void YY(uint8_t* out,
                                                     uint64_t value,
                                                     unsigned int index) {
  out[(index * 8) + 0] = ((value >> 56) & 0xffU);
  out[(index * 8) + 1] = ((value >> 48) & 0xffU);
  out[(index * 8) + 2] = ((value >> 40) & 0xffU);
  out[(index * 8) + 3] = ((value >> 32) & 0xffU);
  out[(index * 8) + 4] = ((value >> 24) & 0xffU);
  out[(index * 8) + 5] = ((value >> 16) & 0xffU);
  out[(index * 8) + 6] = ((value >> 8) & 0xffU);
  out[(index * 8) + 7] = (value & 0xffU);
}

namespace crypto {
namespace hash {
inline namespace implementation {
class SHA1State : public State {
 public:
  struct Raw {
    uint8_t x[BLOCKSIZE];
    uint32_t h[5];
    uint64_t len;
    uint8_t nx;
    bool finalized;
  };

  SHA1State() noexcept;
  SHA1State(const SHA1State& src) noexcept;

  const Algorithm& algorithm() const noexcept override { return SHA1; }
  std::unique_ptr<State> copy() const override;
  void write(const uint8_t* ptr, std::size_t len) override;
  void finalize() override;
  void sum(uint8_t* ptr, std::size_t len) override;
  void reset() override;

 private:
  void block(const uint8_t* ptr, uint64_t len);

  Raw raw_;
};

SHA1State::SHA1State() noexcept { reset(); }

SHA1State::SHA1State(const SHA1State& src) noexcept {
  ::memcpy(&raw_, &src.raw_, sizeof(raw_));
}

std::unique_ptr<State> SHA1State::copy() const {
  return base::backport::make_unique<SHA1State>(*this);
}

void SHA1State::write(const uint8_t* ptr, std::size_t len) {
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

void SHA1State::finalize() {
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

void SHA1State::sum(uint8_t* ptr, std::size_t len) {
  CHECK(raw_.finalized) << ": hash is not finalized";
  CHECK_EQ(len, SUMSIZE);
  Y(ptr, raw_.h[0], 0);
  Y(ptr, raw_.h[1], 1);
  Y(ptr, raw_.h[2], 2);
  Y(ptr, raw_.h[3], 3);
  Y(ptr, raw_.h[4], 4);
}

void SHA1State::reset() {
  ::bzero(&raw_, sizeof(raw_));
  raw_.h[0] = H[0];
  raw_.h[1] = H[1];
  raw_.h[2] = H[2];
  raw_.h[3] = H[3];
  raw_.h[4] = H[4];
}

void SHA1State::block(const uint8_t* ptr, uint64_t len) {
  uint32_t w[80];

  uint32_t h0 = raw_.h[0];
  uint32_t h1 = raw_.h[1];
  uint32_t h2 = raw_.h[2];
  uint32_t h3 = raw_.h[3];
  uint32_t h4 = raw_.h[4];

  uint32_t a, b, c, d, e, f, k, temp;
  unsigned int i;

  while (len >= BLOCKSIZE) {
    i = 0;
    while (i < 16) {
      w[i] = X(ptr, i);
      ++i;
    }
    while (i < 80) {
      w[i] = L(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
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
      temp = L(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = L(b, 30);
      b = a;
      a = temp;
    }
    for (i = 20; i < 40; ++i) {
      f = F1(b, c, d);
      k = 0x6ed9eba1U;
      temp = L(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = L(b, 30);
      b = a;
      a = temp;
    }
    for (i = 40; i < 60; ++i) {
      f = F2(b, c, d);
      k = 0x8f1bbcdcU;
      temp = L(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = L(b, 30);
      b = a;
      a = temp;
    }
    for (i = 60; i < 80; ++i) {
      f = F1(b, c, d);
      k = 0xca62c1d6U;
      temp = L(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = L(b, 30);
      b = a;
      a = temp;
    }

    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;

    len -= BLOCKSIZE;
  }
  DCHECK_EQ(len, 0U);

  raw_.h[0] = h0;
  raw_.h[1] = h1;
  raw_.h[2] = h2;
  raw_.h[3] = h3;
  raw_.h[4] = h4;
}

std::unique_ptr<State> new_sha1() {
  return base::backport::make_unique<SHA1State>();
}
}  // inline namespace implementation

const Algorithm SHA1 = {
    ID::sha1, "SHA-1", BLOCKSIZE, SUMSIZE, Security::broken, new_sha1, nullptr,
};

}  // namespace hash
}  // namespace crypto
