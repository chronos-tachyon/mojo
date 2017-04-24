// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/cbc.h"

#include <cstring>

#include "base/logging.h"
#include "crypto/cipher/cbc_internal.h"
#include "crypto/primitives.h"

namespace crypto {
namespace cipher {
inline namespace implementation {
class CBCCrypter : public Crypter {
 public:
  CBCCrypter(std::unique_ptr<BlockCrypter> block, base::Bytes iv);
  bool is_streaming() const noexcept override { return false; }
  bool is_seekable() const noexcept override { return false; }
  uint16_t block_size() const noexcept override { return block_->block_size(); }
  void encrypt(base::MutableBytes dst, base::Bytes src) override;
  void decrypt(base::MutableBytes dst, base::Bytes src) override;

 private:
  std::unique_ptr<BlockCrypter> block_;
  std::vector<uint8_t> iv_;
  std::vector<uint8_t> scratch_;
};

CBCCrypter::CBCCrypter(std::unique_ptr<BlockCrypter> block, base::Bytes iv)
    : block_(CHECK_NOTNULL(std::move(block))) {
  if (iv.size() != block_size())
    throw std::invalid_argument("invalid IV size for CBC mode");
  iv_.resize(iv.size());
  ::memcpy(iv_.data(), iv.data(), iv.size());
  scratch_.resize(iv.size());
}

void CBCCrypter::encrypt(base::MutableBytes dst, base::Bytes src) {
  cbc_encrypt(iv_, dst, src, [this](base::MutableBytes dst, base::Bytes src) {
    block_->block_encrypt(dst, src);
  });
}

void CBCCrypter::decrypt(base::MutableBytes dst, base::Bytes src) {
  cbc_decrypt(iv_, scratch_, dst, src,
              [this](base::MutableBytes dst, base::Bytes src) {
                block_->block_decrypt(dst, src);
              });
}
}  // inline namespace implementation

std::unique_ptr<Crypter> new_cbc(std::unique_ptr<BlockCrypter> block,
                                 base::Bytes iv) {
  return base::backport::make_unique<CBCCrypter>(std::move(block), iv);
}
}  // namespace cipher
}  // namespace crypto

static const crypto::BlockCipherMode CBC = {
    16,                        // iv_size
    crypto::Security::strong,  // security
    0,                         // flags
    "CBC",                     // name
    crypto::cipher::new_cbc,   // newfn
};

static void init() __attribute__((constructor));
static void init() { crypto::register_mode(&CBC); }
