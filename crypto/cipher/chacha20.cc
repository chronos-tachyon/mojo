// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/chacha20.h"

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

static inline __attribute__((always_inline)) void f(uint32_t& a, uint32_t& b,
                                                    uint32_t& c, uint32_t& d) {
  a += b;
  d ^= a;
  d = ROL32(d, 16);
  c += d;
  b ^= c;
  b = ROL32(b, 12);
  a += b;
  d ^= a;
  d = ROL32(d, 8);
  c += d;
  b ^= c;
  b = ROL32(b, 7);
}

namespace crypto {
namespace cipher {
inline namespace implementation {
struct ChaChaState {
  uint32_t seed[16];
  uint32_t scratch[16];
  uint8_t keystream[CHACHA20_BLOCKSIZE];

  void rekey(const uint8_t* key, uint32_t len);
  void reinit(const uint8_t* nonce, uint32_t len);
  void generate();
};

void ChaChaState::rekey(const uint8_t* key, uint32_t len) {
  const uint8_t* constants;
  const uint8_t* key2;
  if (len == crypto::cipher::CHACHA20_KEYSIZE_FULL) {
    key2 = key + 16;
    constants = SIGMA;
  } else {
    key2 = key;
    constants = TAU;
  }
  seed[0] = RLE32(constants, 0);
  seed[1] = RLE32(constants, 1);
  seed[2] = RLE32(constants, 2);
  seed[3] = RLE32(constants, 3);
  seed[4] = RLE32(key, 0);
  seed[5] = RLE32(key, 1);
  seed[6] = RLE32(key, 2);
  seed[7] = RLE32(key, 3);
  seed[8] = RLE32(key2, 0);
  seed[9] = RLE32(key2, 1);
  seed[10] = RLE32(key2, 2);
  seed[11] = RLE32(key2, 3);
}

void ChaChaState::reinit(const uint8_t* nonce, uint32_t len) {
  CHECK_LE(len, 16U);
  seed[12] = 0;
  seed[13] = 0;
  seed[14] = 0;
  seed[15] = 0;
  uint32_t n = len / 4;
  uint32_t i = 0;
  uint32_t offset = (16 - n);
  while (i < n) {
    seed[i + offset] = RLE32(nonce, i);
    ++i;
  }
}

void ChaChaState::generate() {
  unsigned int i;
  for (i = 0; i < 16; ++i) scratch[i] = seed[i];
  for (i = 0; i < 10; ++i) {
    // Column round
    f(scratch[0], scratch[4], scratch[8], scratch[12]);
    f(scratch[1], scratch[5], scratch[9], scratch[13]);
    f(scratch[2], scratch[6], scratch[10], scratch[14]);
    f(scratch[3], scratch[7], scratch[11], scratch[15]);

    // Diagonal round
    f(scratch[0], scratch[5], scratch[10], scratch[15]);
    f(scratch[1], scratch[6], scratch[11], scratch[12]);
    f(scratch[2], scratch[7], scratch[8], scratch[13]);
    f(scratch[3], scratch[4], scratch[9], scratch[14]);
  }
  for (i = 0; i < 16; ++i) scratch[i] += seed[i];
  for (i = 0; i < 16; ++i) WLE32(keystream, i, scratch[i]);
}

class ChaChaCrypter : public Crypter {
 public:
  ChaChaCrypter(base::Bytes key, base::Bytes nonce);

  bool is_streaming() const noexcept override { return true; }
  bool is_seekable() const noexcept override { return true; }
  uint16_t block_size() const noexcept override { return CHACHA20_BLOCKSIZE; }

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

  crypto::subtle::SecureMemory<ChaChaState> state_;
  uint64_t zero_;
  uint16_t available_;
};

ChaChaCrypter::ChaChaCrypter(base::Bytes key, base::Bytes nonce) {
  switch (key.size()) {
    case CHACHA20_KEYSIZE_HALF:
    case CHACHA20_KEYSIZE_FULL:
      break;

    default:
      throw std::invalid_argument("key size not supported for ChaCha20");
  }

  switch (nonce.size()) {
    case 8:
    case 12:
    case 16:
      break;

    default:
      throw std::invalid_argument("nonce size not supported for ChaCha20");
  }

  state_->rekey(key.data(), key.size());
  state_->reinit(nonce.data(), nonce.size());
  zero_ = fetch_counter();
  available_ = 0;
}

void ChaChaCrypter::encrypt(base::MutableBytes dst, base::Bytes src) noexcept {
  CHECK_GE(dst.size(), src.size());

  uint8_t* dptr = dst.data();
  const uint8_t* sptr = src.data();
  auto len = src.size();
  while (len) {
    if (available_ == 0) next();
    uint16_t n = available_;
    if (available_ > len) n = len;
    auto keypos = CHACHA20_BLOCKSIZE - available_;
    for (uint32_t i = 0; i < n; ++i) {
      dptr[i] = sptr[i] ^ state_->keystream[keypos + i];
    }
    dptr += n;
    sptr += n;
    len -= n;
    available_ -= n;
  }
}

void ChaChaCrypter::decrypt(base::MutableBytes dst, base::Bytes src) noexcept {
  encrypt(dst, src);
}

static base::Result seek_before_start() {
  return base::Result::out_of_range("seek past start of stream");
}

static base::Result seek_after_end() {
  return base::Result::out_of_range("seek past end of stream");
}

base::Result ChaChaCrypter::seek(int64_t pos, int whence) {
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

base::Result ChaChaCrypter::tell(int64_t* pos) {
  CHECK_NOTNULL(pos);
  uint64_t abspos = fetch_position();
  if (abspos & HIGHBIT)
    throw std::overflow_error(
        "stream position cannot be represented as int64_t");
  *pos = abspos;
  return base::Result();
}

void ChaChaCrypter::set_counter(uint64_t value) noexcept {
  YY(state_->seed + 12, value);
}

void ChaChaCrypter::set_position(uint64_t value) noexcept {
  auto x = value / CHACHA20_BLOCKSIZE;
  auto y = value % CHACHA20_BLOCKSIZE;
  set_counter(zero_ + x);
  state_->generate();
  set_counter(zero_ + x + 1);
  available_ = CHACHA20_BLOCKSIZE - y;
}

uint64_t ChaChaCrypter::fetch_counter() const noexcept {
  return XX(state_->seed + 12);
}

uint64_t ChaChaCrypter::fetch_position() const noexcept {
  auto x_plus_1 = fetch_counter() - zero_;
  return (x_plus_1 * CHACHA20_BLOCKSIZE) - available_;
}

void ChaChaCrypter::next() {
  DCHECK_EQ(available_, 0U);
  auto curpos = fetch_counter();
  if (curpos & HIGHBIT) throw std::overflow_error("ChaCha20 counter overflow");

  state_->generate();
  set_counter(fetch_counter() + 1);
  available_ = CHACHA20_BLOCKSIZE;
}
}  // inline namespace implementation

std::unique_ptr<Crypter> new_chacha20(base::Bytes key, base::Bytes nonce) {
  return base::backport::make_unique<ChaChaCrypter>(key, nonce);
}
}  // namespace cipher
}  // namespace crypto

static const crypto::StreamCipher CHACHA20 = {
    crypto::cipher::CHACHA20_BLOCKSIZE,     // block_size
    crypto::cipher::CHACHA20_KEYSIZE_FULL,  // key_size
    crypto::cipher::CHACHA20_NONCESIZE,     // nonce_size
    crypto::Security::secure,               // security
    crypto::StreamCipher::FLAG_SEEKABLE,    // flags
    "ChaCha20",                             // name
    crypto::cipher::new_chacha20,           // newfn
};

static void init() __attribute__((constructor));
static void init() { crypto::register_stream_cipher(&CHACHA20); }
