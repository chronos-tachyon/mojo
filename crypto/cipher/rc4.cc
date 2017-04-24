// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/rc4.h"

#include "base/logging.h"
#include "crypto/primitives.h"
#include "crypto/security.h"

namespace crypto {
namespace cipher {
inline namespace implementation {
struct RC4State {
  uint8_t state[256];
  uint8_t i;
  uint8_t j;

  void rekey(const uint8_t* key, uint32_t len);
  void encrypt(uint8_t* dst, const uint8_t* src, std::size_t len);
};

void RC4State::rekey(const uint8_t* key, uint32_t len) {
  unsigned int x, y;
  for (x = 0; x < 256; ++x) {
    state[x] = x;
  }
  y = 0;
  for (x = 0; x < 256; ++x) {
    y = (y + state[x] + key[x % len]) & 0xff;
    std::swap(state[x], state[y]);
  }
  i = j = 0;
}

void RC4State::encrypt(uint8_t* dst, const uint8_t* src, std::size_t len) {
  for (std::size_t x = 0; x < len; x++) {
    i += 1;
    j += state[i];
    std::swap(state[i], state[j]);
    dst[x] = src[x] ^ state[(state[i] + state[j]) & 0xff];
  }
}

class RC4Crypter : public Crypter {
 public:
  RC4Crypter(base::Bytes key, base::Bytes nonce);

  bool is_streaming() const noexcept override { return true; }
  bool is_seekable() const noexcept override { return false; }
  uint16_t block_size() const noexcept override { return RC4_BLOCKSIZE; }

  void encrypt(base::MutableBytes dst, base::Bytes src) noexcept override;
  void decrypt(base::MutableBytes dst, base::Bytes src) noexcept override;

 private:
  void set_counter(uint64_t value) noexcept;
  void set_position(uint64_t value) noexcept;
  uint64_t fetch_counter() const noexcept;
  uint64_t fetch_position() const noexcept;
  void next();

  crypto::subtle::SecureMemory<RC4State> state_;
};

RC4Crypter::RC4Crypter(base::Bytes key, base::Bytes nonce) {
  if (key.size() < 1 || key.size() > 256) {
    throw std::invalid_argument("key size not supported for RC4");
  }

  if (nonce.size() != 0) {
    throw std::invalid_argument("nonce size not supported for RC4");
  }

  state_->rekey(key.data(), key.size());
}

void RC4Crypter::encrypt(base::MutableBytes dst, base::Bytes src) noexcept {
  CHECK_GE(dst.size(), src.size());
  state_->encrypt(dst.data(), src.data(), src.size());
}

void RC4Crypter::decrypt(base::MutableBytes dst, base::Bytes src) noexcept {
  encrypt(dst, src);
}
}  // inline namespace implementation

std::unique_ptr<Crypter> new_rc4(base::Bytes key, base::Bytes nonce) {
  return base::backport::make_unique<RC4Crypter>(key, nonce);
}
}  // namespace cipher
}  // namespace crypto

static const crypto::StreamCipher RC4 = {
    crypto::cipher::RC4_BLOCKSIZE,  // block_size
    crypto::cipher::RC4_KEYSIZE,    // key_size
    crypto::cipher::RC4_NONCESIZE,  // nonce_size
    crypto::Security::weak,         // security
    0,                              // flags
    "RC4",                          // name
    crypto::cipher::new_rc4,        // newfn
};

static void init() __attribute__((constructor));
static void init() { crypto::register_stream_cipher(&RC4); }
