// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/hash.h"

#include "base/logging.h"

static constexpr std::size_t BLOCKSIZE = 64;
static constexpr std::size_t SHA224_SUMSIZE = 28;
static constexpr std::size_t SHA256_SUMSIZE = 32;

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

static inline __attribute__((always_inline)) uint32_t R(uint32_t x,
                                                        unsigned int c) {
  return (x >> c) | (x << (32 - c));
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
class SHA256State : public State {
 public:
  struct Raw {
    uint8_t x[BLOCKSIZE];
    uint32_t h[8];
    uint64_t len;
    uint8_t nx;
    bool finalized;
  };

  explicit SHA256State(ID id) noexcept;
  SHA256State(const SHA256State& src) noexcept;

  const Algorithm& algorithm() const noexcept override;
  std::unique_ptr<State> copy() const override;
  void write(const uint8_t* ptr, std::size_t len) override;
  void finalize() override;
  void sum(uint8_t* ptr, std::size_t len) override;
  void reset() override;

 private:
  void block(const uint8_t* ptr, uint64_t len);

  Raw raw_;
  ID id_;
};

SHA256State::SHA256State(ID id) noexcept : id_(id) { reset(); }

SHA256State::SHA256State(const SHA256State& src) noexcept : id_(src.id_) {
  ::memcpy(&raw_, &src.raw_, sizeof(raw_));
}

const Algorithm& SHA256State::algorithm() const noexcept {
  switch (id_) {
    case ID::sha224:
      return SHA224;

    default:
      return SHA256;
  }
}

std::unique_ptr<State> SHA256State::copy() const {
  return base::backport::make_unique<SHA256State>(*this);
}

void SHA256State::write(const uint8_t* ptr, std::size_t len) {
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

void SHA256State::finalize() {
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

void SHA256State::sum(uint8_t* ptr, std::size_t len) {
  CHECK(raw_.finalized) << ": hash is not finalized";
  CHECK_EQ(len, size());
  Y(ptr, raw_.h[0], 0);
  Y(ptr, raw_.h[1], 1);
  Y(ptr, raw_.h[2], 2);
  Y(ptr, raw_.h[3], 3);
  Y(ptr, raw_.h[4], 4);
  Y(ptr, raw_.h[5], 5);
  Y(ptr, raw_.h[6], 6);
  if (id_ == ID::sha224) return;
  Y(ptr, raw_.h[7], 7);
}

void SHA256State::reset() {
  ::bzero(&raw_, sizeof(raw_));
  const uint32_t* h;
  switch (id_) {
    case ID::sha224:
      h = SHA224_H;
      break;

    default:
      h = SHA256_H;
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

void SHA256State::block(const uint8_t* ptr, uint64_t len) {
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

  while (len >= BLOCKSIZE) {
    i = 0;
    while (i < 16) {
      w[i] = X(ptr, i);
      ++i;
    }
    while (i < 64) {
      s0 = w[i - 15];
      s1 = w[i - 2];
      s0 = R(s0, 7) ^ R(s0, 18) ^ (s0 >> 3);
      s1 = R(s1, 17) ^ R(s1, 19) ^ (s1 >> 10);
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
      s1 = R(e, 6) ^ R(e, 11) ^ R(e, 25);
      ch = (e & f) ^ ((~e) & g);
      temp1 = h + s1 + ch + K[i] + w[i];
      s0 = R(a, 2) ^ R(a, 13) ^ R(a, 22);
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

    len -= BLOCKSIZE;
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

std::unique_ptr<State> new_sha224() {
  return base::backport::make_unique<SHA256State>(ID::sha224);
}

std::unique_ptr<State> new_sha256() {
  return base::backport::make_unique<SHA256State>(ID::sha256);
}
}  // inline namespace implementation

const Algorithm SHA224 = {
    ID::sha224,       "SHA-224",  BLOCKSIZE, SHA224_SUMSIZE,
    Security::secure, new_sha224, nullptr,
};

const Algorithm SHA256 = {
    ID::sha256,       "SHA-256",  BLOCKSIZE, SHA256_SUMSIZE,
    Security::secure, new_sha256, nullptr,
};

}  // namespace hash
}  // namespace crypto
