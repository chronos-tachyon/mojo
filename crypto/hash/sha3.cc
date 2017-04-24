// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/sha3.h"

#include "base/logging.h"
#include "crypto/hash/keccak.h"

static constexpr std::size_t MAX_RATE = crypto::hash::SHAKE128_BLOCKSIZE;

static inline std::size_t min_length(std::size_t a, std::size_t b) {
  return (a < b) ? a : b;
}

namespace crypto {
namespace hash {

inline namespace implementation {
class SHA3Hasher : public Hasher {
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

  enum class ID : unsigned int {
    sha3_224 = 1,
    sha3_256 = 2,
    sha3_384 = 3,
    sha3_512 = 4,
    shake128 = 5,
    shake256 = 6,
  };

  explicit SHA3Hasher(ID id, uint16_t rate, uint16_t size) noexcept;
  SHA3Hasher(const SHA3Hasher& src) noexcept;

  uint16_t block_size() const noexcept override { return rate_; }
  uint16_t output_size() const noexcept override { return size_; }
  bool is_sponge() const noexcept override;

  std::unique_ptr<Hasher> copy() const override;
  void reset() override;
  void write(base::Bytes in) override;
  void finalize() override;
  void sum(base::MutableBytes out) override;

 private:
  Raw raw_;
  ID id_;
  uint16_t rate_;  // input block size
  uint16_t size_;  // output length
};

SHA3Hasher::SHA3Hasher(ID id, uint16_t rate, uint16_t size) noexcept
    : id_(id),
      rate_(rate),
      size_(size) {
  reset();
}

SHA3Hasher::SHA3Hasher(const SHA3Hasher& src) noexcept : id_(src.id_),
                                                         rate_(src.rate_),
                                                         size_(src.size_) {
  ::memcpy(&raw_, &src.raw_, sizeof(raw_));
}

bool SHA3Hasher::is_sponge() const noexcept {
  switch (id_) {
    case ID::shake128:
    case ID::shake256:
      return true;

    default:
      return false;
  }
}

std::unique_ptr<Hasher> SHA3Hasher::copy() const {
  return base::backport::make_unique<SHA3Hasher>(*this);
}

void SHA3Hasher::reset() { ::bzero(&raw_, sizeof(raw_)); }

void SHA3Hasher::write(base::Bytes in) {
  CHECK(!raw_.finalized) << ": hash is finalized";

  const auto* ptr = in.data();
  auto len = in.size();

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

void SHA3Hasher::finalize() {
  CHECK(!raw_.finalized) << ": hash is finalized";

  // Pad as "M || S || 10*1", where "S" depends on algorithm.
  // Bits are specified LSB-first!
  uint8_t b;
  switch (id_) {
    case ID::shake128:
    case ID::shake256:
      b = 0x1f;  // 11 11 10 00
      break;

    default:
      b = 0x06;  // 01 10 00 00
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

void SHA3Hasher::sum(base::MutableBytes out) {
  CHECK(raw_.finalized) << ": hash is not finalized";

  auto* ptr = out.data();
  auto len = out.size();

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
}  // inline namespace implementation

std::unique_ptr<Hasher> new_sha3_224() {
  return base::backport::make_unique<SHA3Hasher>(
      SHA3Hasher::ID::sha3_224, SHA3_224_BLOCKSIZE, SHA3_224_SUMSIZE);
}

std::unique_ptr<Hasher> new_sha3_256() {
  return base::backport::make_unique<SHA3Hasher>(
      SHA3Hasher::ID::sha3_256, SHA3_256_BLOCKSIZE, SHA3_256_SUMSIZE);
}

std::unique_ptr<Hasher> new_sha3_384() {
  return base::backport::make_unique<SHA3Hasher>(
      SHA3Hasher::ID::sha3_384, SHA3_384_BLOCKSIZE, SHA3_384_SUMSIZE);
}

std::unique_ptr<Hasher> new_sha3_512() {
  return base::backport::make_unique<SHA3Hasher>(
      SHA3Hasher::ID::sha3_512, SHA3_512_BLOCKSIZE, SHA3_512_SUMSIZE);
}

std::unique_ptr<Hasher> new_shake128(uint16_t d) {
  if (d == 0) d = SHAKE128_SUGGESTED_SUMSIZE;
  return base::backport::make_unique<SHA3Hasher>(SHA3Hasher::ID::shake128,
                                                 SHAKE128_BLOCKSIZE, d);
}

static std::unique_ptr<Hasher> new_shake128_fixed() { return new_shake128(); }

static std::unique_ptr<Hasher> new_shake128_variable(uint16_t d) {
  return new_shake128(d);
}

std::unique_ptr<Hasher> new_shake256(uint16_t d) {
  if (d == 0) d = SHAKE256_SUGGESTED_SUMSIZE;
  return base::backport::make_unique<SHA3Hasher>(SHA3Hasher::ID::shake256,
                                                 SHAKE256_BLOCKSIZE, d);
}

static std::unique_ptr<Hasher> new_shake256_fixed() { return new_shake256(); }

static std::unique_ptr<Hasher> new_shake256_variable(uint16_t d) {
  return new_shake256(d);
}
}  // namespace hash
}  // namespace crypto

using CH = crypto::Hash;

static const crypto::Hash SHA3_224 = {
    crypto::hash::SHA3_224_BLOCKSIZE,  // block_size
    crypto::hash::SHA3_224_SUMSIZE,    // output_size
    crypto::Security::secure,          // security
    0,                                 // flags
    "SHA3-224",                        // name
    crypto::hash::new_sha3_224,        // newfn
    nullptr,                           // varfn
};

static const crypto::Hash SHA3_256 = {
    crypto::hash::SHA3_256_BLOCKSIZE,  // block_size
    crypto::hash::SHA3_256_SUMSIZE,    // output_size
    crypto::Security::secure,          // security
    0,                                 // flags
    "SHA3-256",                        // name
    crypto::hash::new_sha3_256,        // newfn
    nullptr,                           // varfn
};

static const crypto::Hash SHA3_384 = {
    crypto::hash::SHA3_384_BLOCKSIZE,  // block_size
    crypto::hash::SHA3_384_SUMSIZE,    // output_size
    crypto::Security::secure,          // security
    0,                                 // flags
    "SHA3-384",                        // name
    crypto::hash::new_sha3_384,        // newfn
    nullptr,                           // varfn
};

static const crypto::Hash SHA3_512 = {
    crypto::hash::SHA3_512_BLOCKSIZE,  // block_size
    crypto::hash::SHA3_512_SUMSIZE,    // output_size
    crypto::Security::secure,          // security
    0,                                 // flags
    "SHA3-512",                        // name
    crypto::hash::new_sha3_512,        // newfn
    nullptr,                           // varfn
};

static const crypto::Hash SHAKE128 = {
    crypto::hash::SHAKE128_BLOCKSIZE,          // block_size
    crypto::hash::SHAKE128_SUGGESTED_SUMSIZE,  // output_size
    crypto::Security::secure,                  // security
    CH::FLAG_VARLEN | CH::FLAG_SPONGE,         // flags
    "SHAKE128",                                // name
    crypto::hash::new_shake128_fixed,          // newfn
    crypto::hash::new_shake128_variable,       // varfn
};

static const crypto::Hash SHAKE256 = {
    crypto::hash::SHAKE256_BLOCKSIZE,          // block_size
    crypto::hash::SHAKE256_SUGGESTED_SUMSIZE,  // output_size
    crypto::Security::secure,                  // security
    CH::FLAG_VARLEN | CH::FLAG_SPONGE,         // flags
    "SHAKE256",                                // name
    crypto::hash::new_shake256_fixed,          // newfn
    crypto::hash::new_shake256_variable,       // varfn
};

static void init() __attribute__((constructor));
static void init() {
  crypto::register_hash(&SHA3_224);
  crypto::register_hash(&SHA3_256);
  crypto::register_hash(&SHA3_384);
  crypto::register_hash(&SHA3_512);
  crypto::register_hash(&SHAKE128);
  crypto::register_hash(&SHAKE256);
}
