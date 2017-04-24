// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/ecb.h"

#include <cstring>

#include "base/logging.h"

namespace crypto {
namespace cipher {
inline namespace implementation {
class ECBCrypter : public Crypter {
 public:
  ECBCrypter(std::unique_ptr<BlockCrypter> block, base::Bytes iv);
  bool is_streaming() const noexcept override { return false; }
  bool is_seekable() const noexcept override { return true; }
  uint16_t block_size() const noexcept override { return block_->block_size(); }
  void encrypt(base::MutableBytes dst, base::Bytes src) override;
  void decrypt(base::MutableBytes dst, base::Bytes src) override;
  base::Result seek(int64_t pos, int whence) override;
  base::Result tell(int64_t* pos) override;

 private:
  std::unique_ptr<BlockCrypter> block_;
};

ECBCrypter::ECBCrypter(std::unique_ptr<BlockCrypter> block, base::Bytes iv)
    : block_(CHECK_NOTNULL(std::move(block))) {
  if (iv.size() != 0)
    throw std::invalid_argument("invalid IV size for ECB mode");
}

void ECBCrypter::encrypt(base::MutableBytes dst, base::Bytes src) {
  CHECK_GE(dst.size(), src.size());

  uint8_t* dptr = dst.data();
  const uint8_t* sptr = src.data();
  std::size_t len = src.size();
  std::size_t blksz = block_->block_size();
  while (len >= blksz) {
    block_->block_encrypt(base::MutableBytes(dptr, blksz),
                          base::Bytes(sptr, blksz));
    dptr += blksz;
    sptr += blksz;
    len -= blksz;
  }
  CHECK_EQ(len, 0U);
}

void ECBCrypter::decrypt(base::MutableBytes dst, base::Bytes src) {
  CHECK_GE(dst.size(), src.size());

  uint8_t* dptr = dst.data();
  const uint8_t* sptr = src.data();
  std::size_t len = src.size();
  std::size_t blksz = block_->block_size();
  while (len >= blksz) {
    block_->block_decrypt(base::MutableBytes(dptr, blksz),
                          base::Bytes(sptr, blksz));
    dptr += blksz;
    sptr += blksz;
    len -= blksz;
  }
  CHECK_EQ(len, 0U);
}

base::Result ECBCrypter::seek(int64_t pos, int whence) {
  return base::Result();
}

base::Result ECBCrypter::tell(int64_t* pos) {
  CHECK_NOTNULL(pos);
  *pos = 0;
  return base::Result();
}
}  // inline namespace implementation

std::unique_ptr<Crypter> new_ecb(std::unique_ptr<BlockCrypter> block,
                                 base::Bytes iv) {
  return base::backport::make_unique<ECBCrypter>(std::move(block), iv);
}
}  // namespace cipher
}  // namespace crypto

static const crypto::BlockCipherMode ECB = {
    0,                                       // iv_size
    crypto::Security::broken,                // security
    crypto::BlockCipherMode::FLAG_SEEKABLE,  // flags
    "ECB",                                   // name
    crypto::cipher::new_ecb,                 // newfn
};

static void init() __attribute__((constructor));
static void init() { crypto::register_mode(&ECB); }
