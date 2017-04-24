// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_CIPHER_CTR_INTERNAL_H
#define CRYPTO_CIPHER_CTR_INTERNAL_H

#include <climits>
#include <cstring>
#include <utility>

#include "base/bytes.h"
#include "base/logging.h"
#include "base/result.h"
#include "crypto/primitives.h"

namespace {

static constexpr uint32_t HIGHBIT32 = 0x80000000UL;
static constexpr uint64_t HIGHBIT64 = 0x8000000000000000ULL;

static base::Result seektell_overflow() {
  return base::Result::out_of_range("stream position cannot be represented as int64_t");
}

static base::Result seek_before_start() {
  return base::Result::out_of_range("seek past start of stream");
}

static base::Result seek_after_end() {
  return base::Result::out_of_range("seek past end of stream");
}

template <typename Functor>
struct CTRGuts {
  Functor f;
  base::MutableBytes iv;
  base::MutableBytes keystream;
  uint64_t zero;
  uint16_t available;
  bool is_32bit;

  CTRGuts(Functor f) noexcept : f(std::move(f)), zero(0), available(0), is_32bit(false) {}

  void encrypt(base::MutableBytes dst, base::Bytes src) {
    CHECK_GE(dst.size(), src.size());

    uint8_t* dptr = dst.data();
    const uint8_t* sptr = src.data();
    std::size_t len = src.size();
    std::size_t blksz = iv.size();
    while (len) {
      if (available == 0) next();
      uint16_t n = available;
      if (available > len) n = len;
      auto keypos = blksz - available;
      crypto::primitives::memxor(dptr, sptr, keystream.data() + keypos, n);
      dptr += n;
      sptr += n;
      len -= n;
      available -= n;
    }
  }

  base::Result seek(int64_t pos, int whence) {
    uint64_t newpos, oldpos, abspos;
    bool neg, valid;

    if (pos < 0) {
      neg = true;
      abspos = uint64_t(-(pos + 1)) + 1;
    } else {
      neg = false;
      abspos = uint64_t(pos);
    }

    switch (whence) {
      case SEEK_SET:
        if (neg) return seek_before_start();
        newpos = abspos;
        break;

      case SEEK_CUR:
        std::tie(valid, oldpos) = fetch_position();
        if (!valid) return seektell_overflow();
        if (neg) {
          if (abspos > oldpos) return seek_before_start();
          newpos = oldpos - abspos;
        } else {
          newpos = oldpos + abspos;
          if (newpos & HIGHBIT64) return seek_after_end();
        }
        break;

      case SEEK_END:
        if (!neg) return seek_after_end();
        newpos = HIGHBIT64 - abspos;
        break;

      default:
        return base::Result::invalid_argument("invalid whence");
    }
    if (!set_position(newpos)) return seektell_overflow();
    return base::Result();
  }

  base::Result tell(int64_t* pos) {
    CHECK_NOTNULL(pos);
    uint64_t abspos;
    bool valid;
    std::tie(valid, abspos) = fetch_position();
    if (!valid) return seektell_overflow();
    if (abspos & HIGHBIT64) return seektell_overflow();
    *pos = abspos;
    return base::Result();
  }

  void set_counter(uint64_t value) noexcept {
    if (is_32bit) {
      crypto::primitives::WBE32(iv.data() + iv.size() - 4, 0, value);
    } else {
      crypto::primitives::WBE64(iv.data() + iv.size() - 8, 0, value);
    }
  }

  bool set_position(uint64_t value) noexcept {
    uint64_t blksz = iv.size();
    auto x = value / blksz;
    auto y = value % blksz;
    uint64_t highbit = is_32bit ? HIGHBIT32 : HIGHBIT64;
    if (x >= highbit) return false;
    set_counter(zero + x);
    f(keystream, iv);
    set_counter(zero + x + 1);
    available = blksz - y;
    return true;
  }

  uint64_t fetch_counter() const noexcept {
    if (is_32bit) {
      return crypto::primitives::RBE32(iv.data() + iv.size() - 4, 0);
    } else {
      return crypto::primitives::RBE64(iv.data() + iv.size() - 8, 0);
    }
  }

  std::pair<bool, uint64_t> fetch_position() const noexcept {
    uint64_t blksz = iv.size();
    uint64_t x_plus_1 = fetch_counter() - zero;
    if (x_plus_1 > UINT64_MAX / blksz) return std::make_pair(false, 0);
    return std::make_pair(true, (x_plus_1 * blksz) - available);
  }

  void next() {
    DCHECK_EQ(available, 0U);
    auto ctr = fetch_counter();
    uint64_t highbit = is_32bit ? HIGHBIT32 : HIGHBIT64;
    if (ctr >= highbit) throw std::overflow_error("CTR mode overflow");

    std::size_t blksz = iv.size();
    f(keystream, iv);
    set_counter(ctr + 1);
    available = blksz;
  }
};

}  // anonymous namespace

#endif  // CRYPTO_CIPHER_CTR_INTERNAL_H
