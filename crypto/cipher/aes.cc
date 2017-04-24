// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/aes.h"

#include <cstdint>

#include <iomanip>
#include <iostream>
#include <sstream>

#include "base/logging.h"
#include "crypto/cipher/aes_internal.h"
#include "crypto/cipher/cbc_internal.h"
#include "crypto/cipher/ctr_internal.h"
#include "crypto/cipher/gcm_internal.h"
#include "crypto/security.h"

namespace crypto {
namespace cipher {
inline namespace implementation {
void aes_expand_key(AESState* state, const uint8_t* key, std::size_t len) {
  switch (len) {
    case AES128_KEYSIZE:
    case AES192_KEYSIZE:
    case AES256_KEYSIZE:
      break;

    default:
      throw std::invalid_argument("invalid key size for AES");
  }
  if (aes_acceleration_available())
    aes_accelerated_expand_key(state, key, len);
  else
    aes_generic_expand_key(state, key, len);
}

void aes_expand_key(AESState* state, base::Bytes key) {
  aes_expand_key(state, key.data(), key.size());
}

static AESState* aes_setup(crypto::subtle::SecureMemory<AESState>& state,
                           base::Bytes key) {
  AESState* ptr = state.get();
  aes_expand_key(ptr, key.data(), key.size());
  return ptr;
}

void aes_encrypt(const AESState* state, uint8_t* dst, const uint8_t* src,
                 std::size_t len) noexcept {
  if (aes_acceleration_available())
    aes_accelerated_encrypt(state, dst, src, len);
  else
    aes_generic_encrypt(state, dst, src, len);
}

void aes_encrypt(const AESState* state, base::MutableBytes dst,
                 base::Bytes src) noexcept {
  DCHECK_GE(dst.size(), src.size());
  aes_encrypt(state, dst.data(), src.data(), src.size());
}

void aes_decrypt(const AESState* state, uint8_t* dst, const uint8_t* src,
                 std::size_t len) noexcept {
  if (aes_acceleration_available())
    aes_accelerated_decrypt(state, dst, src, len);
  else
    aes_generic_decrypt(state, dst, src, len);
}

void aes_decrypt(const AESState* state, base::MutableBytes dst,
                 base::Bytes src) noexcept {
  DCHECK_GE(dst.size(), src.size());
  aes_decrypt(state, dst.data(), src.data(), src.size());
}

struct AESEncryptFunctor {
  const AESState* state;

  AESEncryptFunctor(AESState* s) noexcept : state(s) {}

  void operator()(base::MutableBytes dst, base::Bytes src) const {
    aes_encrypt(state, dst, src);
  }
};

struct AESDecryptFunctor {
  const AESState* state;

  AESDecryptFunctor(AESState* s) noexcept : state(s) {}

  void operator()(base::MutableBytes dst, base::Bytes src) const {
    aes_decrypt(state, dst, src);
  }
};

class AESBlockCrypter : public BlockCrypter {
 public:
  AESBlockCrypter(base::Bytes key);
  uint16_t block_size() const noexcept override { return AES_BLOCKSIZE; }
  void block_encrypt(base::MutableBytes dst, base::Bytes src) const
      noexcept override;
  void block_decrypt(base::MutableBytes dst, base::Bytes src) const
      noexcept override;

 private:
  crypto::subtle::SecureMemory<AESState> state_;
};

AESBlockCrypter::AESBlockCrypter(base::Bytes key) { aes_setup(state_, key); }

void AESBlockCrypter::block_encrypt(base::MutableBytes dst,
                                    base::Bytes src) const noexcept {
  aes_encrypt(state_.get(), dst, src);
}

void AESBlockCrypter::block_decrypt(base::MutableBytes dst,
                                    base::Bytes src) const noexcept {
  aes_decrypt(state_.get(), dst, src);
}

class AESCBCCrypter : public Crypter {
 public:
  AESCBCCrypter(base::Bytes key, base::Bytes iv);
  bool is_streaming() const noexcept override { return false; }
  bool is_seekable() const noexcept override { return false; }
  uint16_t block_size() const noexcept override { return AES_BLOCKSIZE; }
  void encrypt(base::MutableBytes dst, base::Bytes src) override;
  void decrypt(base::MutableBytes dst, base::Bytes src) override;

 private:
  base::MutableBytes iv() noexcept {
    return base::MutableBytes(state_->iv, sizeof(state_->iv));
  }
  base::MutableBytes scratch() noexcept {
    return base::MutableBytes(state_->scratch, sizeof(state_->scratch));
  }

  crypto::subtle::SecureMemory<AESState> state_;
};

AESCBCCrypter::AESCBCCrypter(base::Bytes key, base::Bytes iv) {
  aes_setup(state_, key);

  switch (iv.size()) {
    case AES_BLOCKSIZE:
      break;

    default:
      throw std::invalid_argument("invalid IV size for CBC mode");
  }
  ::memcpy(state_->iv, iv.data(), AES_BLOCKSIZE);
}

void AESCBCCrypter::encrypt(base::MutableBytes dst, base::Bytes src) {
  AESEncryptFunctor f(state_.get());
  cbc_encrypt(iv(), dst, src, f);
}

void AESCBCCrypter::decrypt(base::MutableBytes dst, base::Bytes src) {
  AESDecryptFunctor f(state_.get());
  cbc_decrypt(iv(), scratch(), dst, src, f);
}

class AESCTRCrypter : public Crypter {
 public:
  AESCTRCrypter(base::Bytes key, base::Bytes iv);
  bool is_streaming() const noexcept override { return false; }
  bool is_seekable() const noexcept override { return false; }
  uint16_t block_size() const noexcept override { return AES_BLOCKSIZE; }
  void encrypt(base::MutableBytes dst, base::Bytes src) override;
  void decrypt(base::MutableBytes dst, base::Bytes src) override;
  base::Result seek(int64_t pos, int whence) override;
  base::Result tell(int64_t* pos) override;

 private:
  crypto::subtle::SecureMemory<AESState> state_;
  CTRGuts<AESEncryptFunctor> ctr_;
};

AESCTRCrypter::AESCTRCrypter(base::Bytes key, base::Bytes iv)
    : ctr_(AESEncryptFunctor(aes_setup(state_, key))) {
  switch (iv.size()) {
    case AES_BLOCKSIZE - 8:
    case AES_BLOCKSIZE:
      break;

    default:
      throw std::invalid_argument("invalid IV size for CTR mode");
  }

  ::bzero(state_->iv, AES_BLOCKSIZE);
  ::memcpy(state_->iv, iv.data(), iv.size());
  ctr_.iv = base::MutableBytes(state_->iv, sizeof(state_->iv));
  ctr_.keystream = base::MutableBytes(state_->scratch, sizeof(state_->scratch));
  ctr_.zero = ctr_.fetch_counter();
}

void AESCTRCrypter::encrypt(base::MutableBytes dst, base::Bytes src) {
  ctr_.encrypt(dst, src);
}

void AESCTRCrypter::decrypt(base::MutableBytes dst, base::Bytes src) {
  ctr_.encrypt(dst, src);
}

base::Result AESCTRCrypter::seek(int64_t pos, int whence) {
  return ctr_.seek(pos, whence);
}

base::Result AESCTRCrypter::tell(int64_t* pos) { return ctr_.tell(pos); }

class AESGCMSealer : public Sealer {
 public:
  AESGCMSealer(base::Bytes key);
  uint16_t nonce_size() const noexcept override { return AES_GCM_NONCESIZE; }
  uint16_t tag_size() const noexcept override { return AES_GCM_TAGSIZE; }
  void seal(Tag* tag, base::MutableBytes ciphertext, base::Bytes plaintext,
            base::Bytes additional, base::Bytes nonce) const override;
  bool unseal(const Tag& tag, base::MutableBytes plaintext,
              base::Bytes ciphertext, base::Bytes additional,
              base::Bytes nonce) const override;

 private:
  crypto::subtle::SecureMemory<AESState> state_;
  GCMKey<AESEncryptFunctor> gcm_;
};

AESGCMSealer::AESGCMSealer(base::Bytes key)
    : gcm_(AESEncryptFunctor(aes_setup(state_, key)), state_->product_table) {}

void AESGCMSealer::seal(Tag* tag, base::MutableBytes ciphertext,
                        base::Bytes plaintext, base::Bytes additional,
                        base::Bytes nonce) const {
  CHECK_NOTNULL(tag);
  tag->set_size(AES_GCM_TAGSIZE);
  GCMState<AESEncryptFunctor> state(&gcm_, nonce);
  state.seal(tag->mutable_data(), ciphertext, plaintext, additional);
}

bool AESGCMSealer::unseal(const Tag& tag, base::MutableBytes plaintext,
                          base::Bytes ciphertext, base::Bytes additional,
                          base::Bytes nonce) const {
  if (tag.size() != AES_GCM_TAGSIZE) return false;
  GCMState<AESEncryptFunctor> state(&gcm_, nonce);
  return state.unseal(tag.data(), plaintext, ciphertext, additional);
}
}  // inline namespace implementation

std::unique_ptr<BlockCrypter> new_aes(base::Bytes key) {
  return base::backport::make_unique<AESBlockCrypter>(key);
}

std::unique_ptr<Crypter> new_aes_cbc(base::Bytes key, base::Bytes iv) {
  return base::backport::make_unique<AESCBCCrypter>(key, iv);
}

std::unique_ptr<Crypter> new_aes_ctr(base::Bytes key, base::Bytes iv) {
  return base::backport::make_unique<AESCTRCrypter>(key, iv);
}

std::unique_ptr<Sealer> new_aes_gcm(base::Bytes key) {
  return base::backport::make_unique<AESGCMSealer>(key);
}
}  // namespace cipher
}  // namespace crypto

static const crypto::BlockCipher AES128 = {
    crypto::cipher::AES_BLOCKSIZE,   // block_size
    crypto::cipher::AES128_KEYSIZE,  // key_size
    crypto::Security::secure,        // security
    0,                               // flags
    "AES-128",                       // name
    crypto::cipher::new_aes,         // newfn
    crypto::cipher::new_aes_cbc,     // cbcfn
    crypto::cipher::new_aes_ctr,     // ctrfn
    crypto::cipher::new_aes_gcm,     // gcmfn
};

static const crypto::BlockCipher AES192 = {
    crypto::cipher::AES_BLOCKSIZE,   // block_size
    crypto::cipher::AES192_KEYSIZE,  // key_size
    crypto::Security::secure,        // security
    0,                               // flags
    "AES-192",                       // name
    crypto::cipher::new_aes,         // newfn
    crypto::cipher::new_aes_cbc,     // cbcfn
    crypto::cipher::new_aes_ctr,     // ctrfn
    crypto::cipher::new_aes_gcm,     // gcmfn
};

static const crypto::BlockCipher AES256 = {
    crypto::cipher::AES_BLOCKSIZE,   // block_size
    crypto::cipher::AES256_KEYSIZE,  // key_size
    crypto::Security::secure,        // security
    0,                               // flags
    "AES-256",                       // name
    crypto::cipher::new_aes,         // newfn
    crypto::cipher::new_aes_cbc,     // cbcfn
    crypto::cipher::new_aes_ctr,     // ctrfn
    crypto::cipher::new_aes_gcm,     // gcmfn
};

static void init() __attribute__((constructor));
static void init() {
  crypto::register_block_cipher(&AES128);
  crypto::register_block_cipher(&AES192);
  crypto::register_block_cipher(&AES256);
}
