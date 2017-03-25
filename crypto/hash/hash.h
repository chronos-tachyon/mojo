// crypto/hash/hash.h - Interface for cryptographically-strong hash functions
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_HASH_HASH_H
#define CRYPTO_HASH_HASH_H

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "base/strings.h"
#include "crypto/common/common.h"
#include "io/writer.h"

namespace crypto {
namespace hash {

using Security = crypto::common::Security;

class State;  // forward declaration

using NewState = std::unique_ptr<State> (*)();
using NewVariableLengthState = std::unique_ptr<State> (*)(unsigned int);

enum class ID : uint32_t {
  md4 = 0x1,
  md5 = 0x2,
  ripemd160 = 0x3,
  sha1 = 0x4,
  sha224 = 0x5,
  sha256 = 0x6,
  sha384 = 0x7,
  sha512 = 0x8,
  sha512_224 = 0x9,
  sha512_256 = 0xa,
  sha3_224 = 0xb,
  sha3_256 = 0xc,
  sha3_384 = 0xd,
  sha3_512 = 0xe,
  shake128 = 0xf,
  shake256 = 0x10,
};

struct Algorithm {
  ID id;
  const char* name;
  uint32_t block_size;  // bytes
  uint32_t size;        // bytes
  Security security;
  NewState newfn;
  NewVariableLengthState newvarlenfn;

  uint32_t block_size_bits() const noexcept { return block_size * 8; }
  uint32_t size_bits() const noexcept { return size * 8; }
  bool is_secure() const noexcept { return security == Security::secure; }
};

extern const Algorithm MD4;
extern const Algorithm MD5;
extern const Algorithm RIPEMD160;
extern const Algorithm SHA1;
extern const Algorithm SHA224;
extern const Algorithm SHA256;
extern const Algorithm SHA384;
extern const Algorithm SHA512;
extern const Algorithm SHA512_224;
extern const Algorithm SHA512_256;
extern const Algorithm SHA3_224;
extern const Algorithm SHA3_256;
extern const Algorithm SHA3_384;
extern const Algorithm SHA3_512;
extern const Algorithm SHAKE128;
extern const Algorithm SHAKE256;

class State {
 protected:
  State() noexcept = default;
  State(const State&) noexcept = default;
  State(State&&) noexcept = default;
  State& operator=(const State&) noexcept = default;
  State& operator=(State&&) noexcept = default;

 public:
  virtual ~State() noexcept = default;
  virtual const Algorithm& algorithm() const noexcept = 0;
  virtual std::unique_ptr<State> copy() const = 0;
  virtual void write(const uint8_t* ptr, std::size_t len) = 0;
  virtual void finalize() = 0;
  virtual void sum(uint8_t* ptr, std::size_t len) = 0;
  virtual void reset() = 0;

  virtual uint32_t block_size() const noexcept {
    return algorithm().block_size;
  }
  virtual uint32_t size() const noexcept { return algorithm().size; }

  uint32_t block_size_bits() const noexcept { return block_size() * 8; }
  uint32_t size_bits() const noexcept { return size() * 8; }

  void write(const char* ptr, std::size_t len) {
    write(reinterpret_cast<const uint8_t*>(ptr), len);
  }

  void write(base::StringPiece in) { write(in.data(), in.size()); }

  std::string sum_binary();
  std::string sum_hex();
  std::string sum_base64();
};

std::vector<const Algorithm*> all(Security min_security);
const Algorithm* by_id(ID id, Security min_security) noexcept;
const Algorithm* by_name(base::StringPiece name,
                         Security min_security) noexcept;

io::Writer statewriter(State* state);

}  // namespace hash
}  // namespace crypto

#endif  // CRYPTO_HASH_HASH_H
