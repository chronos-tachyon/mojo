// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/cfb.h"

#include <cstring>

#include "base/logging.h"

namespace crypto {
namespace cipher {
inline namespace implementation {
class CFBCrypter : public Crypter {
 public:
  CFBCrypter(std::unique_ptr<BlockCrypter> block, base::Bytes iv);
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

CFBCrypter::CFBCrypter(std::unique_ptr<BlockCrypter> block, base::Bytes iv)
    : block_(CHECK_NOTNULL(std::move(block))) {
  if (iv.size() != block_size())
    throw std::invalid_argument("invalid IV size for CFB mode");
  iv_.resize(iv.size());
  ::memcpy(iv_.data(), iv.data(), iv.size());
  scratch_.resize(iv.size());
}

void CFBCrypter::encrypt(base::MutableBytes dst, base::Bytes src) {
  CHECK_GE(dst.size(), src.size());

  uint8_t* dptr = dst.data();
  const uint8_t* sptr = src.data();
  std::size_t len = src.size();
  std::size_t blksz = iv_.size();
  while (len >= blksz) {
    block_->block_encrypt(iv_, iv_);
    for (uint32_t i = 0; i < blksz; ++i) {
      iv_[i] ^= sptr[i];
    }
    ::memcpy(dptr, iv_.data(), blksz);
    dptr += blksz;
    sptr += blksz;
    len -= blksz;
  }
  CHECK_EQ(len, 0U);
}

void CFBCrypter::decrypt(base::MutableBytes dst, base::Bytes src) {
  CHECK_GE(dst.size(), src.size());

  uint8_t* dptr = dst.data();
  const uint8_t* sptr = src.data();
  std::size_t len = src.size();
  std::size_t blksz = iv_.size();
  while (len >= blksz) {
    ::memcpy(scratch_.data(), sptr, blksz);
    block_->block_encrypt(base::MutableBytes(dptr, blksz), iv_);
    for (uint32_t i = 0; i < blksz; ++i) {
      dptr[i] ^= scratch_[i];
    }
    ::memcpy(iv_.data(), scratch_.data(), blksz);
    dptr += blksz;
    sptr += blksz;
    len -= blksz;
  }
  CHECK_EQ(len, 0U);
}
}  // inline namespace implementation

std::unique_ptr<Crypter> new_cfb(std::unique_ptr<BlockCrypter> block,
                                 base::Bytes iv) {
  return base::backport::make_unique<CFBCrypter>(std::move(block), iv);
}
}  // namespace cipher
}  // namespace crypto

static const crypto::BlockCipherMode CFB = {
    16,                        // iv_size
    crypto::Security::strong,  // security
    0,                         // flags
    "CFB",                     // name
    crypto::cipher::new_cfb,   // newfn
};

static void init() __attribute__((constructor));
static void init() { crypto::register_mode(&CFB); }
