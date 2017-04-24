// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/ofb.h"

#include <cstring>

#include "base/logging.h"

namespace crypto {
namespace cipher {
inline namespace implementation {
class OFBCrypter : public Crypter {
 public:
  OFBCrypter(std::unique_ptr<BlockCrypter> block, base::Bytes iv);
  bool is_streaming() const noexcept override { return true; }
  bool is_seekable() const noexcept override { return false; }
  uint16_t block_size() const noexcept override { return block_->block_size(); }
  void encrypt(base::MutableBytes dst, base::Bytes src) override;
  void decrypt(base::MutableBytes dst, base::Bytes src) override;

 private:
  void next();

  std::unique_ptr<BlockCrypter> block_;
  std::vector<uint8_t> iv_;
  uint16_t available_;
};

OFBCrypter::OFBCrypter(std::unique_ptr<BlockCrypter> block, base::Bytes iv)
    : block_(CHECK_NOTNULL(std::move(block))) {
  if (iv.size() != block_size())
    throw std::invalid_argument("invalid IV size for OFB mode");
  iv_.resize(iv.size());
  ::memcpy(iv_.data(), iv.data(), iv.size());
  available_ = iv.size();
}

void OFBCrypter::encrypt(base::MutableBytes dst, base::Bytes src) {
  CHECK_GE(dst.size(), src.size());

  uint8_t* dptr = dst.data();
  const uint8_t* sptr = src.data();
  std::size_t len = src.size();
  std::size_t blksz = iv_.size();
  while (len) {
    if (available_ == 0) next();
    uint16_t n = available_;
    if (available_ > len) n = len;
    auto keypos = blksz - available_;
    for (uint32_t i = 0; i < n; ++i) {
      dptr[i] = sptr[i] ^ iv_[keypos + i];
    }
    dptr += n;
    sptr += n;
    len -= n;
    available_ -= n;
  }
}

void OFBCrypter::decrypt(base::MutableBytes dst, base::Bytes src) {
  encrypt(dst, src);
}

void OFBCrypter::next() {
  DCHECK_EQ(available_, 0U);
  block_->block_encrypt(iv_, iv_);
  available_ = iv_.size();
}
}  // inline namespace implementation

std::unique_ptr<Crypter> new_ofb(std::unique_ptr<BlockCrypter> block,
                                 base::Bytes iv) {
  return base::backport::make_unique<OFBCrypter>(std::move(block), iv);
}
}  // namespace cipher
}  // namespace crypto

static const crypto::BlockCipherMode OFB = {
    16,                                       // iv_size
    crypto::Security::strong,                 // security
    crypto::BlockCipherMode::FLAG_STREAMING,  // flags
    "OFB",                                    // name
    crypto::cipher::new_ofb,                  // newfn
};

static void init() __attribute__((constructor));
static void init() { crypto::register_mode(&OFB); }
