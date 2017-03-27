// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/hash.h"

#include <ostream>

#include "encoding/base64.h"
#include "encoding/hex.h"

namespace crypto {
namespace hash {

static const Algorithm* const ALL[] = {
    nullptr,      // 0x00
    &MD4,         // 0x01
    &MD5,         // 0x02
    &RIPEMD160,   // 0x03
    &SHA1,        // 0x04
    &SHA224,      // 0x05
    &SHA256,      // 0x06
    &SHA384,      // 0x07
    &SHA512,      // 0x08
    &SHA512_224,  // 0x09
    &SHA512_256,  // 0x0a
    &SHA3_224,    // 0x0b
    &SHA3_256,    // 0x0c
    &SHA3_384,    // 0x0d
    &SHA3_512,    // 0x0e
    &SHAKE128,    // 0x0f
    &SHAKE256,    // 0x10
};

static bool filter(const Algorithm* ptr, Security min_security) {
  return ptr && ptr->newfn && (ptr->security >= min_security);
}

std::string State::sum_binary() {
  std::vector<uint8_t> raw;
  raw.resize(size());
  sum(raw.data(), raw.size());
  return std::string(reinterpret_cast<const char*>(raw.data()), raw.size());
}

std::string State::sum_hex() {
  std::vector<uint8_t> raw;
  raw.resize(size());
  sum(raw.data(), raw.size());
  return encode(encoding::HEX, raw.data(), raw.size());
}

std::string State::sum_base64() {
  std::vector<uint8_t> raw;
  raw.resize(size());
  sum(raw.data(), raw.size());
  return encode(encoding::BASE64, raw.data(), raw.size());
}

std::vector<const Algorithm*> all(Security min_security) {
  std::vector<const Algorithm*> out;
  for (const Algorithm* ptr : ALL) {
    if (filter(ptr, min_security)) out.push_back(ptr);
  }
  return out;
}

const Algorithm* by_id(ID id, Security min_security) noexcept {
  const Algorithm* ptr = nullptr;
  switch (id) {
    case ID::md4:
      ptr = &MD4;
      break;

    case ID::md5:
      ptr = &MD5;
      break;

    case ID::ripemd160:
      ptr = &RIPEMD160;
      break;

    case ID::sha1:
      ptr = &SHA1;
      break;

    case ID::sha224:
      ptr = &SHA224;
      break;

    case ID::sha256:
      ptr = &SHA256;
      break;

    case ID::sha384:
      ptr = &SHA384;
      break;

    case ID::sha512:
      ptr = &SHA512;
      break;

    case ID::sha512_224:
      ptr = &SHA512_224;
      break;

    case ID::sha512_256:
      ptr = &SHA512_256;
      break;

    case ID::sha3_224:
      ptr = &SHA3_224;
      break;

    case ID::sha3_256:
      ptr = &SHA3_256;
      break;

    case ID::sha3_384:
      ptr = &SHA3_384;
      break;

    case ID::sha3_512:
      ptr = &SHA3_512;
      break;

    case ID::shake128:
      ptr = &SHAKE128;
      break;

    case ID::shake256:
      ptr = &SHAKE256;
      break;
  }
  if (filter(ptr, min_security))
    return ptr;
  else
    return nullptr;
}

const Algorithm* by_name(base::StringPiece name,
                         Security min_security) noexcept {
  using crypto::common::canonical_name;
  std::string cname = canonical_name(name);
  for (const Algorithm* ptr : ALL) {
    if (filter(ptr, min_security))
      if (cname == canonical_name(ptr->name)) return ptr;
  }
  return nullptr;
}

inline namespace implementation {
class StateWriter : public io::WriterImpl {
 public:
  explicit StateWriter(State* state) noexcept : state_(CHECK_NOTNULL(state)) {}

  std::size_t ideal_block_size() const noexcept { return state_->block_size(); }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
    if (!prologue(task, n, ptr, len)) return;
    state_->write(ptr, len);
    task->finish(base::Result());
  }

  void close(event::Task* task, const base::Options& opts) override {
    if (!prologue(task)) return;
    state_->finalize();
    task->finish(base::Result());
  }

 private:
  State* state_;
};
}  // inline namespace implementation

io::Writer statewriter(State* state) {
  return io::Writer(std::make_shared<StateWriter>(state));
}

}  // namespace hash
}  // namespace crypto
