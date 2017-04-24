// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/salsa20.h"

#include "base/logging.h"
#include "crypto/primitives.h"
#include "crypto/security.h"
#include "crypto/subtle.h"

using crypto::primitives::ROL32;
using crypto::primitives::RLE32;
using crypto::primitives::WLE32;

static constexpr uint8_t SIGMA[] = "expand 32-byte k";
static constexpr uint8_t TAU[] = "expand 16-byte k";

static constexpr uint64_t HIGHBIT = 0x8000000000000000ULL;
static constexpr uint64_t U64MAX = 0xffffffffffffffffULL;
static constexpr uint32_t U32MAX = 0xffffffffUL;

static inline uint64_t XX(const uint32_t* ptr) noexcept {
  if (crypto::primitives::X86OPT) {
    const uint64_t* ptr64 = reinterpret_cast<const uint64_t*>(ptr);
    return *ptr64;
  } else {
    return uint64_t(ptr[0]) | (uint64_t(ptr[1]) << 32);
  }
}

static inline void YY(uint32_t* ptr, uint64_t value) noexcept {
  if (crypto::primitives::X86OPT) {
    uint64_t* ptr64 = reinterpret_cast<uint64_t*>(ptr);
    *ptr64 = value;
  } else {
    ptr[0] = value & U32MAX;
    ptr[1] = (value >> 32) & U32MAX;
  }
}

static inline __attribute__((always_inline)) void f(uint32_t& x, uint32_t y, uint32_t z, unsigned int k) {
  x ^= ROL32(y + z, k);
}

namespace crypto {
namespace cipher {
inline namespace implementation {
struct SalsaState {
  uint32_t seed[16];
  uint32_t scratch[16];
  uint8_t keystream[SALSA20_BLOCKSIZE];

  void rekey(const uint8_t* key, uint32_t len);
  void reinit(const uint8_t* nonce);
  void generate();
};

void SalsaState::rekey(const uint8_t* key, uint32_t len) {
  const uint8_t* constants;
  seed[1] = RLE32(key, 0);
  seed[2] = RLE32(key, 1);
  seed[3] = RLE32(key, 2);
  seed[4] = RLE32(key, 3);
  if (len == crypto::cipher::SALSA20_KEYSIZE_FULL) {
    key += 16;
    constants = SIGMA;
  } else {
    constants = TAU;
  }
  seed[11] = RLE32(key, 0);
  seed[12] = RLE32(key, 1);
  seed[13] = RLE32(key, 2);
  seed[14] = RLE32(key, 3);
  seed[0] = RLE32(constants, 0);
  seed[5] = RLE32(constants, 1);
  seed[10] = RLE32(constants, 2);
  seed[15] = RLE32(constants, 3);
}

void SalsaState::reinit(const uint8_t* nonce) {
  seed[6] = RLE32(nonce, 0);
  seed[7] = RLE32(nonce, 1);
  seed[8] = 0;
  seed[9] = 0;
}

void SalsaState::generate() {
  unsigned int i;
  for (i = 0; i < 16; ++i) scratch[i] = seed[i];
  for (i = 0; i < 10; ++i) {
    f(scratch[4], scratch[0], scratch[12], 7);
    f(scratch[8], scratch[4], scratch[0], 9);
    f(scratch[12], scratch[8], scratch[4], 13);
    f(scratch[0], scratch[12], scratch[8], 18);
    f(scratch[9], scratch[5], scratch[1], 7);
    f(scratch[13], scratch[9], scratch[5], 9);
    f(scratch[1], scratch[13], scratch[9], 13);
    f(scratch[5], scratch[1], scratch[13], 18);
    f(scratch[14], scratch[10], scratch[6], 7);
    f(scratch[2], scratch[14], scratch[10], 9);
    f(scratch[6], scratch[2], scratch[14], 13);
    f(scratch[10], scratch[6], scratch[2], 18);
    f(scratch[3], scratch[15], scratch[11], 7);
    f(scratch[7], scratch[3], scratch[15], 9);
    f(scratch[11], scratch[7], scratch[3], 13);
    f(scratch[15], scratch[11], scratch[7], 18);
    f(scratch[1], scratch[0], scratch[3], 7);
    f(scratch[2], scratch[1], scratch[0], 9);
    f(scratch[3], scratch[2], scratch[1], 13);
    f(scratch[0], scratch[3], scratch[2], 18);
    f(scratch[6], scratch[5], scratch[4], 7);
    f(scratch[7], scratch[6], scratch[5], 9);
    f(scratch[4], scratch[7], scratch[6], 13);
    f(scratch[5], scratch[4], scratch[7], 18);
    f(scratch[11], scratch[10], scratch[9], 7);
    f(scratch[8], scratch[11], scratch[10], 9);
    f(scratch[9], scratch[8], scratch[11], 13);
    f(scratch[10], scratch[9], scratch[8], 18);
    f(scratch[12], scratch[15], scratch[14], 7);
    f(scratch[13], scratch[12], scratch[15], 9);
    f(scratch[14], scratch[13], scratch[12], 13);
    f(scratch[15], scratch[14], scratch[13], 18);
  }
  for (i = 0; i < 16; ++i) scratch[i] += seed[i];
  for (i = 0; i < 16; ++i) WLE32(keystream, i, scratch[i]);
}

class SalsaCrypter : public Crypter {
 public:
  SalsaCrypter(base::Bytes key, base::Bytes nonce);

  bool is_streaming() const noexcept override { return true; }
  bool is_seekable() const noexcept override { return true; }
  uint16_t block_size() const noexcept override { return SALSA20_BLOCKSIZE; }

  void encrypt(base::MutableBytes dst, base::Bytes src) noexcept override;
  void decrypt(base::MutableBytes dst, base::Bytes src) noexcept override;

  base::Result seek(int64_t pos, int whence) override;
  base::Result tell(int64_t* pos) override;

 private:
  void set_counter(uint64_t value) noexcept;
  void set_position(uint64_t value) noexcept;
  uint64_t fetch_counter() const noexcept;
  uint64_t fetch_position() const noexcept;
  void next();

  crypto::subtle::SecureMemory<SalsaState> state_;
  uint64_t zero_;
  uint16_t available_;
};

SalsaCrypter::SalsaCrypter(base::Bytes key, base::Bytes nonce) {
  switch (key.size()) {
    case SALSA20_KEYSIZE_HALF:
    case SALSA20_KEYSIZE_FULL:
      break;

    default:
      throw std::invalid_argument("key size not supported for Salsa20");
  }

  switch (nonce.size()) {
    case SALSA20_NONCESIZE:
      break;

    default:
      throw std::invalid_argument("nonce size not supported for Salsa20");
  }

  state_->rekey(key.data(), key.size());
  state_->reinit(nonce.data());
  zero_ = fetch_counter();
  available_ = 0;
}

void SalsaCrypter::encrypt(base::MutableBytes dst, base::Bytes src) noexcept {
  CHECK_GE(dst.size(), src.size());

  uint8_t* dptr = dst.data();
  const uint8_t* sptr = src.data();
  auto len = src.size();
  while (len) {
    if (available_ == 0) next();
    uint16_t n = available_;
    if (available_ > len) n = len;
    auto keypos = SALSA20_BLOCKSIZE - available_;
    for (uint32_t i = 0; i < n; ++i) {
      dptr[i] = sptr[i] ^ state_->keystream[keypos + i];
    }
    dptr += n;
    sptr += n;
    len -= n;
    available_ -= n;
  }
}

void SalsaCrypter::decrypt(base::MutableBytes dst, base::Bytes src) noexcept {
  encrypt(dst, src);
}

static base::Result seek_before_start() {
  return base::Result::out_of_range("seek past start of stream");
}

static base::Result seek_after_end() {
  return base::Result::out_of_range("seek past end of stream");
}

base::Result SalsaCrypter::seek(int64_t pos, int whence) {
  uint64_t newpos, oldpos, abspos;
  bool neg;

  if (pos < 0) {
    neg = true;
    abspos = uint64_t(-(pos + 1)) + 1;
  } else {
    neg = false;
    abspos = uint64_t(pos);
  }

  oldpos = fetch_position();
  switch (whence) {
    case SEEK_SET:
      if (neg) return seek_before_start();
      newpos = abspos;
      break;

    case SEEK_CUR:
      if (neg) {
        if (abspos > oldpos) return seek_before_start();
        newpos = oldpos - abspos;
      } else {
        newpos = oldpos + abspos;
        if (newpos & HIGHBIT) return seek_after_end();
      }
      break;

    case SEEK_END:
      if (!neg) return seek_after_end();
      newpos = HIGHBIT - abspos;
      break;

    default:
      return base::Result::invalid_argument("invalid whence");
  }
  set_position(newpos);
  return base::Result();
}

base::Result SalsaCrypter::tell(int64_t* pos) {
  CHECK_NOTNULL(pos);
  uint64_t abspos = fetch_position();
  if (abspos & HIGHBIT)
    throw std::overflow_error(
        "stream position cannot be represented as int64_t");
  *pos = abspos;
  return base::Result();
}

void SalsaCrypter::set_counter(uint64_t value) noexcept {
  YY(state_->seed + 8, value);
}

void SalsaCrypter::set_position(uint64_t value) noexcept {
  auto x = value / SALSA20_BLOCKSIZE;
  auto y = value % SALSA20_BLOCKSIZE;
  set_counter(zero_ + x);
  state_->generate();
  set_counter(zero_ + x + 1);
  available_ = SALSA20_BLOCKSIZE - y;
}

uint64_t SalsaCrypter::fetch_counter() const noexcept {
  return XX(state_->seed + 8);
}

uint64_t SalsaCrypter::fetch_position() const noexcept {
  auto x_plus_1 = fetch_counter() - zero_;
  return (x_plus_1 * SALSA20_BLOCKSIZE) - available_;
}

void SalsaCrypter::next() {
  DCHECK_EQ(available_, 0U);
  auto curpos = fetch_counter();
  if (curpos & HIGHBIT) throw std::overflow_error("Salsa20 counter overflow");

  state_->generate();
  set_counter(fetch_counter() + 1);
  available_ = SALSA20_BLOCKSIZE;
}
}  // inline namespace implementation

std::unique_ptr<Crypter> new_salsa20(base::Bytes key, base::Bytes nonce) {
  return base::backport::make_unique<SalsaCrypter>(key, nonce);
}
}  // namespace cipher
}  // namespace crypto

static const crypto::StreamCipher SALSA20 = {
    crypto::cipher::SALSA20_BLOCKSIZE,     // block_size
    crypto::cipher::SALSA20_KEYSIZE_FULL,  // key_size
    crypto::cipher::SALSA20_NONCESIZE,     // nonce_size
    crypto::Security::secure,              // security
    crypto::StreamCipher::FLAG_SEEKABLE,   // flags
    "Salsa20",                             // name
    crypto::cipher::new_salsa20,           // newfn
};

static void init() __attribute__((constructor));
static void init() { crypto::register_stream_cipher(&SALSA20); }
