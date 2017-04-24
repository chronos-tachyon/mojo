// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/gcm.h"

#include "crypto/cipher/gcm_internal.h"
#include "crypto/primitives.h"
#include "crypto/subtle.h"

using crypto::primitives::RBE64;

namespace crypto {
namespace cipher {

static std::unique_ptr<BlockCrypter> validate_blockcipher(
    std::unique_ptr<BlockCrypter> ptr) {
  if (ptr->block_size() != GCM_BLOCKSIZE)
    throw std::invalid_argument(
        "this implementation of GCM only supports 128-bit block ciphers");
  return std::move(ptr);
}

inline namespace implementation {
class GCMSealer : public Sealer {
 public:
  GCMSealer(std::unique_ptr<BlockCrypter> block);
  uint16_t nonce_size() const noexcept override { return GCM_NONCESIZE; }
  uint16_t tag_size() const noexcept override { return GCM_TAGSIZE; }
  void seal(Tag* tag, base::MutableBytes ciphertext, base::Bytes plaintext,
            base::Bytes additional, base::Bytes nonce) const override;
  bool unseal(const Tag& tag, base::MutableBytes plaintext,
              base::Bytes ciphertext, base::Bytes additional,
              base::Bytes nonce) const override;

 private:
  struct Functor {
    BlockCrypter* block;

    Functor(BlockCrypter* b) noexcept : block(b) {}

    void operator()(base::MutableBytes dst, base::Bytes src) const {
      block->block_encrypt(dst, src);
    }
  };

  struct Storage {
    GCMElement product_table[16];
  };

  std::unique_ptr<BlockCrypter> block_;
  crypto::subtle::SecureMemory<Storage> storage_;
  GCMKey<Functor> gcm_;
};

GCMSealer::GCMSealer(std::unique_ptr<BlockCrypter> block)
    : block_(validate_blockcipher(std::move(block))),
      gcm_(Functor(block_.get()), storage_->product_table) {}

void GCMSealer::seal(Tag* tag, base::MutableBytes ciphertext,
                     base::Bytes plaintext, base::Bytes additional,
                     base::Bytes nonce) const {
  CHECK_NOTNULL(tag);
  tag->set_size(GCM_TAGSIZE);
  GCMState<Functor> state(&gcm_, nonce);
  state.seal(tag->mutable_data(), ciphertext, plaintext, additional);
}

bool GCMSealer::unseal(const Tag& tag, base::MutableBytes plaintext,
                       base::Bytes ciphertext, base::Bytes additional,
                       base::Bytes nonce) const {
  if (tag.size() != GCM_TAGSIZE) return false;
  GCMState<Functor> state(&gcm_, nonce);
  return state.unseal(tag.data(), plaintext, ciphertext, additional);
}
}  // inline namespace implementation

std::unique_ptr<Sealer> new_gcm(std::unique_ptr<BlockCrypter> block) {
  return base::backport::make_unique<GCMSealer>(std::move(block));
}
}  // namespace cipher
}  // namespace crypto
