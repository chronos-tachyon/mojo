// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/cipher/ctr.h"

#include <cstring>

#include "base/logging.h"
#include "crypto/cipher/ctr_internal.h"
#include "crypto/primitives.h"

using crypto::primitives::RBE64;
using crypto::primitives::WBE64;

namespace crypto {
namespace cipher {
inline namespace implementation {
class CTRCrypter : public Crypter {
 public:
  CTRCrypter(std::unique_ptr<BlockCrypter> block, base::Bytes iv);
  bool is_streaming() const noexcept override { return true; }
  bool is_seekable() const noexcept override { return true; }
  uint16_t block_size() const noexcept override { return block_->block_size(); }
  void encrypt(base::MutableBytes dst, base::Bytes src) override;
  void decrypt(base::MutableBytes dst, base::Bytes src) override;
  base::Result seek(int64_t pos, int whence) override;
  base::Result tell(int64_t* pos) override;

 private:
  struct Functor {
    CTRCrypter* self;

    Functor(CTRCrypter* self) noexcept : self(self) {}

    void operator()(base::MutableBytes dst, base::Bytes src) const {
      self->block_->block_encrypt(dst, src);
    }
  };

  std::unique_ptr<BlockCrypter> block_;
  std::vector<uint8_t> iv_;
  std::vector<uint8_t> keystream_;
  CTRGuts<Functor> ctr_;
};

CTRCrypter::CTRCrypter(std::unique_ptr<BlockCrypter> block, base::Bytes iv)
    : block_(CHECK_NOTNULL(std::move(block))), ctr_(Functor(this)) {
  std::size_t blksz = block_size();

  if (blksz < 8)
    throw std::invalid_argument(
        "cipher is not compatible with CTR mode");

  bool is_32bit = (blksz < 16);
  std::size_t ctrsz = is_32bit ? 4 : 8;

  if (iv.size() != blksz && iv.size() != (blksz - ctrsz))
    throw std::invalid_argument("invalid IV size for CTR mode");

  iv_.resize(blksz);
  keystream_.resize(blksz);
  ::memcpy(iv_.data(), iv.data(), iv.size());
  ctr_.iv = base::MutableBytes(iv_.data(), iv_.size());
  ctr_.keystream = base::MutableBytes(keystream_.data(), keystream_.size());
  ctr_.is_32bit = is_32bit;
  ctr_.zero = ctr_.fetch_counter();
}

void CTRCrypter::encrypt(base::MutableBytes dst, base::Bytes src) {
  ctr_.encrypt(dst, src);
}

void CTRCrypter::decrypt(base::MutableBytes dst, base::Bytes src) {
  ctr_.encrypt(dst, src);
}

base::Result CTRCrypter::seek(int64_t pos, int whence) {
  return ctr_.seek(pos, whence);
}

base::Result CTRCrypter::tell(int64_t* pos) { return ctr_.tell(pos); }
}  // inline namespace implementation

std::unique_ptr<Crypter> new_ctr(std::unique_ptr<BlockCrypter> block,
                                 base::Bytes iv) {
  return base::backport::make_unique<CTRCrypter>(std::move(block), iv);
}
}  // namespace cipher
}  // namespace crypto

using BCM = crypto::BlockCipherMode;

static const crypto::BlockCipherMode CTR = {
    8,                                         // iv_size
    crypto::Security::strong,                  // security
    BCM::FLAG_STREAMING | BCM::FLAG_SEEKABLE,  // flags
    "CTR",                                     // name
    crypto::cipher::new_ctr,                   // newfn
};

static void init() __attribute__((constructor));
static void init() { crypto::register_mode(&CTR); }
