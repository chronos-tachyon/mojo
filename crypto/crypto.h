// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_CRYPTO_H
#define CRYPTO_CRYPTO_H

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "base/bytes.h"
#include "base/chars.h"
#include "base/result.h"
#include "crypto/security.h"
#include "crypto/subtle.h"
#include "io/writer.h"

namespace crypto {

// Represents a MAC or other fixed-length authentication tag.
class Tag {
 public:
  // Tag can be constructed from a byte buffer.
  explicit Tag(base::Bytes src) : v_(src.begin(), src.end()) {}

  // Tag can be move constructed from a byte vector.
  explicit Tag(std::vector<uint8_t>&& src) noexcept : v_(std::move(src)) {}

  // Tag is default constructible, copyable, and moveable.
  Tag() = default;
  Tag(const Tag&) = default;
  Tag(Tag&&) noexcept = default;
  Tag& operator=(const Tag&) = default;
  Tag& operator=(Tag&&) noexcept = default;

  void set_size(std::size_t n) noexcept {
    v_.resize(n);
    ::bzero(v_.data(), n);
  }

  std::size_t size() const noexcept { return v_.size(); }
  const uint8_t* data() const noexcept { return v_.data(); }
  uint8_t* mutable_data() noexcept { return v_.data(); }

  base::Bytes bytes() const noexcept { return v_; }
  base::MutableBytes mutable_bytes() noexcept { return v_; }

  // Provides constant-time comparison of two Tags.
  friend bool operator==(const Tag& a, const Tag& b) {
    const uint8_t* aptr = a.v_.data();
    const uint8_t* bptr = b.v_.data();
    std::size_t alen = a.v_.size();
    std::size_t blen = b.v_.size();
    if (alen != blen) return false;
    return crypto::subtle::consttime_eq(aptr, bptr, blen);
  }
  friend bool operator!=(const Tag& a, const Tag& b) { return !(a == b); }

 private:
  std::vector<uint8_t> v_;
};

// Provides an interface to a low-level hash algorithm.
class Hasher {
 protected:
  Hasher() noexcept = default;
  Hasher(const Hasher&) noexcept = default;
  Hasher(Hasher&&) noexcept = default;

 public:
  virtual ~Hasher() noexcept = default;

  // Returns the block size of the hash in bytes.
  //
  // For best performance, |write()| calls should be aligned to the block size.
  //
  virtual uint16_t block_size() const noexcept = 0;

  // Returns the size in bytes of the hash's output.
  //
  // Algorithms using the "sponge" construction can output any number of bytes;
  // in such a case, this is the recommended minimum output size.
  //
  virtual uint16_t output_size() const noexcept = 0;

  // Returns true iff this hash uses the "sponge" construction, and can
  // therefore generate an arbitrarily long output.
  virtual bool is_sponge() const noexcept = 0;

  // Creates and returns a copy of this Hasher's current state.
  virtual std::unique_ptr<Hasher> copy() const = 0;

  // Resets this Hasher to its initial state, i.e. the state that this Hasher
  // originally had before the first call to |write()|.
  virtual void reset() = 0;

  // Writes the given data to the hash state.
  //
  // Ignoring |reset()|, it is an error to call this method after |finalize()|.
  //
  virtual void write(base::Bytes data) = 0;

  // Performs final processing on the hash state.
  //
  // Ignoring |reset()|, it is an error to call this method more than once.
  //
  virtual void finalize() = 0;

  // Reads the hash sum from the hash state.
  //
  // It is an error to call this method before calling |finalize()|.
  //
  // For standard hashes:
  //
  //   The length of |out| must be equal to the |output_size()| of the hash.
  //   This method may be called at most once.
  //
  // For sponge hashes:
  //
  //   The hash sum is an infinite stream of bytes, and each call to this
  //   method reads the next available |out.size()| bytes in the stream.
  //
  //   For best security, it is recommended that the caller read at least the
  //   first |output_size()| bytes of the stream.
  //
  virtual void sum(base::MutableBytes out) = 0;

  // Provides a synchronous io::Writer for hashing data.
  //
  // Closing the io::Writer is the same as calling |finalize()|.
  //
  io::Writer writer();

  Hasher& operator=(const Hasher&) = delete;
  Hasher& operator=(Hasher&&) = delete;
};

// Provides an interface to a low-level block cipher algorithm.
class BlockCrypter {
 protected:
  BlockCrypter() noexcept = default;

 public:
  virtual ~BlockCrypter() noexcept = default;

  // Returns the block size of the cipher.
  virtual uint16_t block_size() const noexcept = 0;

  // Encrypts one block of data.
  //
  // Length of |src| must be an exact multiple of |block_size()|.
  // Length of |dst| must equal or exceed the length of |src|.
  // Only the first |src.size()| bytes of |dst| will be touched.
  //
  // |dst| may equal |src|.  Other than that, they must not overlap.
  //
  // WARNING: DO NOT CALL THIS METHOD DIRECTLY!
  // To use a block cipher safely, wrap it in a block cipher mode.
  //
  virtual void block_encrypt(base::MutableBytes dst, base::Bytes src) const = 0;

  // Decrypts one block of data.
  //
  // Length of |src| must be an exact multiple of |block_size()|.
  // Length of |dst| must equal or exceed the length of |src|.
  // Only the first |src.size()| bytes of |dst| will be touched.
  //
  // |dst| may equal |src|.  Other than that, they must not overlap.
  //
  // WARNING: DO NOT CALL THIS METHOD DIRECTLY!
  // To use a block cipher safely, wrap it in a block cipher mode.
  //
  virtual void block_decrypt(base::MutableBytes dst, base::Bytes src) const = 0;

  BlockCrypter(const BlockCrypter&) = delete;
  BlockCrypter(BlockCrypter&&) = delete;
  BlockCrypter& operator=(const BlockCrypter&) = delete;
  BlockCrypter& operator=(BlockCrypter&&) noexcept = delete;
};

// Provides an interface to a low-level stream cipher algorithm, or to a
// low-level block cipher algorithm wrapped in a block cipher mode.
class Crypter {
 protected:
  Crypter() noexcept = default;

 public:
  virtual ~Crypter() noexcept = default;

  // Returns true iff this is a streaming cipher.
  //
  // All stream ciphers are streaming, as are block ciphers wrapped in certain
  // stream-capable modes (such as CTR mode).
  //
  virtual bool is_streaming() const noexcept = 0;

  // Returns true iff this cipher supports |seek()| and |tell()|.
  virtual bool is_seekable() const noexcept = 0;

  // Returns the block size of the cipher.
  //
  // For non-streaming ciphers, the inputs provided to |encrypt()| and
  // |decrypt()| MUST have lengths that are multiples of this number.
  //
  // For streaming ciphers, that requirement is relaxed, but best performance
  // is achieved by operating on |block_size()| boundaries.
  //
  virtual uint16_t block_size() const noexcept = 0;

  // Encrypts some data.
  //
  // |dst| may equal |src|.  Other than that, they must not overlap.
  //
  // |dst| must be at least as long as |src|.  If |dst| is longer, then only
  // the first |src.size()| bytes of |dst| will be touched.
  //
  virtual void encrypt(base::MutableBytes dst, base::Bytes src) = 0;

  // Decrypts some data.
  //
  // |dst| may equal |src|.  Other than that, they must not overlap.
  //
  // |dst| must be at least as long as |src|.  If |dst| is longer, then only
  // the first |src.size()| bytes of |dst| will be touched.
  //
  virtual void decrypt(base::MutableBytes dst, base::Bytes src) = 0;

  // Seeks the cipher to the specified byte position.  The |whence|
  // argument is interpreted as for lseek(2).
  //
  // NOTE: Most ciphers are not seekable!
  //
  virtual base::Result seek(int64_t pos, int whence);

  // Returns the current position of the cipher's stream.
  //
  // NOTE: Most ciphers are not seekable!
  //
  virtual base::Result tell(int64_t* pos);

  Crypter(const Crypter&) = delete;
  Crypter(Crypter&&) = delete;
  Crypter& operator=(const Crypter&) = delete;
  Crypter& operator=(Crypter&&) noexcept = delete;
};

// Provides an interface to a MAC algorithm with fixed key.
//
//   MAC = Message Authentication Code
//   https://en.wikipedia.org/wiki/Message_authentication_code
//
class Signer {
 protected:
  Signer() noexcept = default;

 public:
  virtual ~Signer() noexcept = default;

  // Returns the size of the required nonce.
  //
  // Most Signers do not require a nonce and return 0 here.
  //
  virtual uint16_t nonce_size() const noexcept = 0;

  // Returns the size of the authenticator tag.
  virtual uint16_t tag_size() const noexcept = 0;

  // Produces the authentication tag for the message.
  virtual void sign(Tag* tag, base::Bytes data, base::Bytes nonce) const = 0;

  // Convenience method that directly returns the tag.
  Tag sign(base::Bytes data, base::Bytes nonce) const {
    Tag tag;
    sign(&tag, data, nonce);
    return tag;
  }

  Signer(const Signer&) = delete;
  Signer(Signer&&) = delete;
  Signer& operator=(const Signer&) = delete;
  Signer& operator=(Signer&&) = delete;
};

// Provides an interface to an AEAD algorithm with fixed key.
//
//   AEAD = Authenticated Encryption with Associated Data
//   https://en.wikipedia.org/wiki/Authenticated_encryption
//
class Sealer {
 protected:
  Sealer() noexcept = default;

 public:
  virtual ~Sealer() noexcept = default;

  // Returns the size of the required nonce.
  virtual uint16_t nonce_size() const noexcept = 0;

  // Returns the size of the AEAD authenticator tag.
  virtual uint16_t tag_size() const noexcept = 0;

  // Seals a message.
  //
  // Inputs:
  //
  //   |plaintext| is the secret to be protected (encrypted and signed).
  //
  //   |additional| is some additional non-secret data to be signed.
  //
  //   |nonce| is an additional non-secret unique value.
  //   It must be at least |nonce_size()| bytes long.
  //
  // Outputs:
  //
  //   |tag| is the authentication tag which seals the data.
  //
  //   |ciphertext| is the encrypted version of |plaintext|.
  //   It must be at least as long as |plaintext|.
  //   It may be equal to |plaintext|, but otherwise must not overlap with it.
  //   It must be distinct from, and non-overlapping with, |additional|.
  //   It must be distinct from, and non-overlapping with, |nonce|.
  //
  virtual void seal(Tag* tag, base::MutableBytes ciphertext,
                    base::Bytes plaintext, base::Bytes additional,
                    base::Bytes nonce) const = 0;

  // Opens a sealed message.
  //
  // Inputs:
  //
  //   |ciphertext| is the encrypted data to be unsealed.
  //
  //   |additional| is the additional non-secret data to be verified.
  //
  //   |nonce| is an additional non-secret unique value.
  //   It must be at least |nonce_size()| bytes long.
  //
  // Outputs:
  //
  //   |tag| is the authentication tag which proves that the sealed data was
  //   not tampered with.
  //
  //   |plaintext| is the decrypted version of |ciphertext|.
  //   It must be at least as long as |ciphertext|.
  //   It may be equal to |ciphertext|, but otherwise must not overlap with it.
  //   It must be distinct from, and non-overlapping with, |additional|.
  //   It must be distinct from, and non-overlapping with, |nonce|.
  //   It is only valid if verification of |tag| succeeds.
  //
  //   Returns true iff verification and encryption were successful.
  //
  virtual bool unseal(const Tag& tag, base::MutableBytes plaintext,
                      base::Bytes ciphertext, base::Bytes additional,
                      base::Bytes nonce) const = 0;

  // Convenience method that directly returns the tag.
  Tag seal(base::MutableBytes ciphertext, base::Bytes plaintext,
           base::Bytes additional, base::Bytes nonce) const {
    Tag tag;
    seal(&tag, ciphertext, plaintext, additional, nonce);
    return tag;
  }

  Sealer(const Sealer&) = delete;
  Sealer(Sealer&&) = delete;
  Sealer& operator=(const Sealer&) = delete;
  Sealer& operator=(Sealer&&) = delete;
};

using NewHasher = std::unique_ptr<Hasher> (*)();
using NewVariableLengthHasher = std::unique_ptr<Hasher> (*)(uint16_t);
using NewBlockCrypter = std::unique_ptr<BlockCrypter> (*)(base::Bytes key);
using NewBlockCrypterForMode = std::unique_ptr<Crypter> (*)(
    std::unique_ptr<BlockCrypter> block, base::Bytes iv);
using NewGCM = std::unique_ptr<Sealer> (*)(base::Bytes key);
using NewCrypter = std::unique_ptr<Crypter> (*)(base::Bytes key,
                                                base::Bytes iv);

struct Hash {
  enum {
    // Indicates that the Hash supports variable output lengths.
    FLAG_VARLEN = (1U << 0),

    // Indicates that the Hash uses the "sponge" construction, i.e. that its
    // output length is unbounded. The default length (newfn), or the length
    // provided at construction time (varfn), is merely a suggested minimum.
    FLAG_SPONGE = (1U << 1),
  };

  uint16_t block_size;
  uint16_t output_size;
  Security security;
  uint8_t flags;
  const char* name;
  NewHasher newfn;
  NewVariableLengthHasher varfn;
};

struct BlockCipher {
  enum {};

  uint16_t block_size;
  uint16_t key_size;
  Security security;
  uint8_t flags;
  const char* name;
  NewBlockCrypter newfn;
  NewCrypter cbcfn;
  NewCrypter ctrfn;
  NewGCM gcmfn;
};

struct BlockCipherMode {
  enum {
    // Indicates that the BlockCipherMode is capable of seeking.
    FLAG_SEEKABLE = (1U << 0),

    // Indicates that the BlockCipherMode is streaming, i.e. that the input
    // need not lie on a block_size boundary AND that |decrypt()| is
    // indistinguishable from |encrypt()|.
    FLAG_STREAMING = (1U << 1),
  };

  uint16_t iv_size;  // relative to 128-bit (16-byte) block_size
  Security security;
  uint8_t flags;
  const char* name;
  NewBlockCrypterForMode newfn;
};

struct StreamCipher {
  enum {
    // Indicates that the StreamCipher is capable of seeking.
    FLAG_SEEKABLE = (1U << 0),
  };

  uint16_t block_size;
  uint16_t key_size;
  uint16_t nonce_size;
  Security security;
  uint8_t flags;
  const char* name;
  NewCrypter newfn;
};

std::vector<const Hash*> all_hashes(Security min);
base::Result find_hash(const Hash** out, base::Chars name, Security min);
void register_hash(const Hash* ptr);

std::vector<const BlockCipher*> all_block_ciphers(Security min);
base::Result find_block_cipher(const BlockCipher** out, base::Chars name,
                               Security min);
void register_block_cipher(const BlockCipher* ptr);

std::vector<const BlockCipherMode*> all_modes(Security min);
base::Result find_mode(const BlockCipherMode** out, base::Chars name,
                       Security min);
void register_mode(const BlockCipherMode* ptr);

std::vector<const StreamCipher*> all_stream_ciphers(Security min);
base::Result find_stream_cipher(const StreamCipher** out, base::Chars name,
                                Security min);
void register_stream_cipher(const StreamCipher* ptr);

base::Result new_hash(std::unique_ptr<Hasher>* out, base::Chars name,
                      Security min);
base::Result new_crypter(std::unique_ptr<Crypter>* out, base::Chars name,
                         Security min, base::Bytes key, base::Bytes iv);

}  // namespace crypto

#endif  // CRYPTO_CRYPTO_H
