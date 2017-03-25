// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/hash/hash.h"

#include <ostream>

namespace crypto {
namespace hash {

static constexpr char HEX[] = "0123456789abcdef";
static constexpr char BASE64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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

  std::string hex;
  hex.reserve(2 * raw.size());
  for (uint8_t byte : raw) {
    hex.push_back(HEX[byte >> 4]);
    hex.push_back(HEX[byte & 15]);
  }
  return hex;
}

std::string State::sum_base64() {
  std::vector<uint8_t> raw;
  raw.resize(size());
  sum(raw.data(), raw.size());

  std::size_t x = raw.size() / 3;
  std::size_t m = x * 4;
  std::size_t n = x * 3;
  std::size_t d = raw.size() - n;
  if (d) m += 4;

  std::string b64;
  b64.reserve(m);

  std::size_t i;
  uint32_t val;
  uint8_t byte0, byte1, byte2;

  for (i = 0; i < n; i += 3) {
    byte0 = raw[i];
    byte1 = raw[i + 1];
    byte2 = raw[i + 2];
    val = (byte0 << 16) | (byte1 << 8) | byte2;
    b64.push_back(BASE64[(val >> 18) & 0x3f]);
    b64.push_back(BASE64[(val >> 12) & 0x3f]);
    b64.push_back(BASE64[(val >> 6) & 0x3f]);
    b64.push_back(BASE64[val & 0x3f]);
  }

  switch (d) {
    case 0:
      break;

    case 1:
      byte0 = raw[i];
      val = (byte0 << 16);
      b64.push_back(BASE64[(val >> 18) & 0x3f]);
      b64.push_back(BASE64[(val >> 12) & 0x3f]);
      b64.push_back('=');
      b64.push_back('=');
      break;

    case 2:
      byte0 = raw[i];
      byte1 = raw[i + 1];
      val = (byte0 << 16) | (byte1 << 8);
      b64.push_back(BASE64[(val >> 18) & 0x3f]);
      b64.push_back(BASE64[(val >> 12) & 0x3f]);
      b64.push_back(BASE64[(val >> 6) & 0x3f]);
      b64.push_back('=');
      break;
  }

  return b64;
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
