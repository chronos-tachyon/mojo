// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/hash.h"

#include "base/logging.h"
#include "crypto/hash/keccak.h"

static constexpr std::size_t SHA3_224_RATE = 144;
static constexpr std::size_t SHA3_256_RATE = 136;
static constexpr std::size_t SHA3_384_RATE = 104;
static constexpr std::size_t SHA3_512_RATE = 72;
static constexpr std::size_t SHAKE128_RATE = 168;
static constexpr std::size_t SHAKE256_RATE = 136;
static constexpr std::size_t MAX_RATE = SHAKE128_RATE;

static constexpr std::size_t SHA3_224_SIZE = 28;
static constexpr std::size_t SHA3_256_SIZE = 32;
static constexpr std::size_t SHA3_384_SIZE = 48;
static constexpr std::size_t SHA3_512_SIZE = 64;
static constexpr std::size_t SHAKE128_SUGGESTED_SIZE = 32;
static constexpr std::size_t SHAKE256_SUGGESTED_SIZE = 64;

static inline std::size_t min_length(std::size_t a, std::size_t b) {
  return (a < b) ? a : b;
}

namespace crypto {
namespace hash {

inline namespace implementation {
class SHA3State : public State {
 public:
  struct Raw {
    // Stores the Keccak permutation state (5 × 5 × 64 bits)
    uint64_t state[25];

    // Stores partial data for write()/sum() calls not on a rate boundary
    uint8_t x[MAX_RATE];

    // Stores the position within x
    // - Absorbing / write()-ing:
    //    x[0:nx] are filled
    //    x[nx:] are empty and available for write()
    //    invariant: 0 ≤ nx < rate_
    // - Squeezing / sum()-ing:
    //    x[0:nx] have already been seen by sum() already
    //    x[nx:] are waiting to be returned to sum()
    //    invariant: 0 < nx ≤ rate_
    uint8_t nx;

    // Has finalize been called?
    // - False iff we are absorbing / write()-ing
    // - True iff we are squeezing / sum()-ing
    bool finalized;
  };

  explicit SHA3State(ID id, unsigned int rate, unsigned int size) noexcept;
  SHA3State(const SHA3State& src) noexcept;

  const Algorithm& algorithm() const noexcept override;
  uint32_t block_size() const noexcept override { return rate_; }
  uint32_t size() const noexcept override { return size_; }
  std::unique_ptr<State> copy() const override;
  void write(const uint8_t* ptr, std::size_t len) override;
  void finalize() override;
  void sum(uint8_t* ptr, std::size_t len) override;
  void reset() override;

 private:
  Raw raw_;
  ID id_;
  unsigned int rate_;  // input block size
  unsigned int size_;  // output length
};

SHA3State::SHA3State(ID id, unsigned int rate, unsigned int size) noexcept
    : id_(id),
      rate_(rate),
      size_(size) {
  reset();
}

SHA3State::SHA3State(const SHA3State& src) noexcept : id_(src.id_),
                                                      rate_(src.rate_),
                                                      size_(src.size_) {
  ::memcpy(&raw_, &src.raw_, sizeof(raw_));
}

const Algorithm& SHA3State::algorithm() const noexcept {
  switch (id_) {
    case ID::sha3_224:
      return SHA3_224;

    case ID::sha3_256:
      return SHA3_256;

    case ID::sha3_384:
      return SHA3_384;

    case ID::sha3_512:
      return SHA3_512;

    case ID::shake128:
      return SHAKE128;

    default:
      return SHAKE256;
  }
}

std::unique_ptr<State> SHA3State::copy() const {
  return base::backport::make_unique<SHA3State>(*this);
}

void SHA3State::write(const uint8_t* ptr, std::size_t len) {
  CHECK(!raw_.finalized) << ": hash is finalized";

  unsigned int nx = raw_.nx;
  if (nx) {
    auto n = min_length(rate_ - nx, len);
    ::memcpy(raw_.x + nx, ptr, n);
    nx += n;
    ptr += n;
    len -= n;
    if (nx == rate_) {
      keccak_f1600_xor_in(raw_.state, raw_.x, rate_);
      keccak_f1600_permute(raw_.state);
      nx = 0;
    }
  }
  while (len >= rate_) {
    keccak_f1600_xor_in(raw_.state, ptr, rate_);
    keccak_f1600_permute(raw_.state);
    ptr += rate_;
    len -= rate_;
  }
  if (len) {
    DCHECK_EQ(nx, 0U);
    ::memcpy(raw_.x, ptr, len);
    nx = len;
  }
  raw_.nx = nx;
}

void SHA3State::finalize() {
  CHECK(!raw_.finalized) << ": hash is finalized";

  // Pad as "M || S || 10*1", where "S" depends on algorithm.
  // Bits are specified LSB-first!
  uint8_t b;
  switch (id_) {
    case ID::sha3_224:
    case ID::sha3_256:
    case ID::sha3_384:
    case ID::sha3_512:
      b = 0x06;  // 01 10 00 00
      break;

    default:
      b = 0x1f;  // 11 11 10 00
      break;
  }

  unsigned int nx = raw_.nx;
  raw_.x[nx] = b;
  ++nx;
  while (nx < rate_) {
    raw_.x[nx] = 0;
    ++nx;
  }
  raw_.x[rate_ - 1] = 0x80;  // 00 00 00 01
  keccak_f1600_xor_in(raw_.state, raw_.x, rate_);
  keccak_f1600_permute(raw_.state);
  raw_.finalized = true;
  raw_.nx = rate_;  // all bytes in x have been consumed
}

void SHA3State::sum(uint8_t* ptr, std::size_t len) {
  CHECK(raw_.finalized) << ": hash is not finalized";

  unsigned int nx = raw_.nx;
  if (nx < rate_) {
    auto n = min_length(rate_ - nx, len);
    ::memcpy(ptr, raw_.x + nx, n);
    nx += n;
    ptr += n;
    len -= n;
  }
  while (len >= rate_) {
    keccak_f1600_copy_out(ptr, rate_, raw_.state);
    keccak_f1600_permute(raw_.state);
    ptr += rate_;
    len -= rate_;
  }
  if (len) {
    DCHECK_EQ(nx, rate_);
    keccak_f1600_copy_out(raw_.x, rate_, raw_.state);
    keccak_f1600_permute(raw_.state);
    ::memcpy(ptr, raw_.x, len);
    nx = len;
  }
  raw_.nx = nx;
}

void SHA3State::reset() { ::bzero(&raw_, sizeof(raw_)); }

std::unique_ptr<State> new_sha3_224() {
  return base::backport::make_unique<SHA3State>(ID::sha3_224, SHA3_224_RATE,
                                                SHA3_224_SIZE);
}

std::unique_ptr<State> new_sha3_256() {
  return base::backport::make_unique<SHA3State>(ID::sha3_256, SHA3_256_RATE,
                                                SHA3_256_SIZE);
}

std::unique_ptr<State> new_sha3_384() {
  return base::backport::make_unique<SHA3State>(ID::sha3_384, SHA3_384_RATE,
                                                SHA3_384_SIZE);
}

std::unique_ptr<State> new_sha3_512() {
  return base::backport::make_unique<SHA3State>(ID::sha3_512, SHA3_512_RATE,
                                                SHA3_512_SIZE);
}

std::unique_ptr<State> new_shake128(unsigned int d) {
  return base::backport::make_unique<SHA3State>(ID::shake128, SHAKE128_RATE, d);
}

std::unique_ptr<State> new_shake128_suggested() {
  return new_shake128(SHAKE128_SUGGESTED_SIZE);
}

std::unique_ptr<State> new_shake256(unsigned int d) {
  return base::backport::make_unique<SHA3State>(ID::shake256, SHAKE256_RATE, d);
}

std::unique_ptr<State> new_shake256_suggested() {
  return new_shake256(SHAKE256_SUGGESTED_SIZE);
}
}  // inline namespace implementation

const Algorithm SHA3_224 = {
    ID::sha3_224,     "SHA3-224",   SHA3_224_RATE, SHA3_224_SIZE,
    Security::secure, new_sha3_224, nullptr,
};

const Algorithm SHA3_256 = {
    ID::sha3_256,     "SHA3-256",   SHA3_256_RATE, SHA3_256_SIZE,
    Security::secure, new_sha3_256, nullptr,
};

const Algorithm SHA3_384 = {
    ID::sha3_384,     "SHA3-384",   SHA3_384_RATE, SHA3_384_SIZE,
    Security::secure, new_sha3_384, nullptr,
};

const Algorithm SHA3_512 = {
    ID::sha3_512,     "SHA3-512",   SHA3_512_RATE, SHA3_512_SIZE,
    Security::secure, new_sha3_512, nullptr,
};

const Algorithm SHAKE128 = {
    ID::shake128,     "SHAKE128",
    SHAKE128_RATE,    SHAKE128_SUGGESTED_SIZE,
    Security::secure, new_shake128_suggested,
    new_shake128,
};

const Algorithm SHAKE256 = {
    ID::shake256,     "SHAKE256",
    SHAKE256_RATE,    SHAKE256_SUGGESTED_SIZE,
    Security::secure, new_shake256_suggested,
    new_shake256,
};

}  // namespace hash
}  // namespace crypto
