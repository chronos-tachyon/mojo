// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/crypto.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "base/logging.h"

static bool streq(const char* a, const char* b) noexcept {
  return ::strcmp(a, b) == 0;
}

static base::Result parse_uint64(uint64_t* out, base::Chars in) {
  CHECK_NOTNULL(out);
  if (in.empty()) return base::Result::invalid_argument("empty string");
  uint64_t value = 0;
  while (!in.empty()) {
    char ch = in.front();
    in.remove_prefix(1);

    if (ch < '0' || ch > '9')
      return base::Result::invalid_argument("invalid character");
    uint8_t x = (ch - '0');
    if (value > UINT64_MAX / 10) return base::Result::out_of_range("overflow");
    if (value * 10 > UINT64_MAX - x) return base::Result::out_of_range("overflow");
    value = (value * 10) + x;
  }
  *out = value;
  return base::Result();
}

// Makes sausage out of an algorithm name.  Sausages may be compared for
// equality, enabling human-friendly matching of algorithm names.
static std::string canonical_name(base::Chars in) {
  std::string out;
  out.reserve(in.size());
  for (char ch : in) {
    if (ch >= '0' && ch <= '9') {
      out.push_back(ch);
    } else if (ch >= 'a' && ch <= 'z') {
      out.push_back(ch);
    } else if (ch >= 'A' && ch <= 'Z') {
      out.push_back(ch + ('a' - 'A'));
    }
  }
  return out;
}

namespace crypto {

static std::mutex g_mu;
static std::vector<const Hash*>* g_hash = nullptr;
static std::vector<const BlockCipher*>* g_block = nullptr;
static std::vector<const BlockCipherMode*>* g_mode = nullptr;
static std::vector<const StreamCipher*>* g_stream = nullptr;

template <typename T>
static std::vector<const T*> all_impl(std::vector<const T*>* gptr,
                                      Security min) {
  std::unique_lock<std::mutex> lock(g_mu);
  std::vector<const T*> out;
  if (gptr) {
    out.reserve(gptr->size());
    for (const T* ptr : *gptr) {
      if (ptr->security >= min) out.push_back(ptr);
    }
  }
  return out;
}

template <typename T>
static base::Result find_impl(std::vector<const T*>* gptr, const char* type,
                              const T** out, base::Chars name, Security min) {
  CHECK_NOTNULL(out);
  std::unique_lock<std::mutex> lock(g_mu);
  if (gptr) {
    auto cname = canonical_name(name);
    for (const T* ptr : *gptr) {
      if (canonical_name(ptr->name) == cname) {
        if (ptr->security >= min) {
          *out = ptr;
          return base::Result();
        } else {
          return base::Result::unavailable(type, " \"", ptr->name,
                                           "\" is not secure");
        }
      }
    }
  }
  return base::Result::not_found(type, " \"", name, "\" was not found");
}

template <typename T>
static void register_impl(std::vector<const T*>** gptrptr, const T* ptr) {
  CHECK_NOTNULL(gptrptr);
  std::unique_lock<std::mutex> lock(g_mu);
  if (!*gptrptr) *gptrptr = new std::vector<const T*>;
  auto& vec = **gptrptr;
  vec.push_back(ptr);
  std::sort(vec.begin(), vec.end(), [](const T* a, const T* b) {
    return ::strcmp(a->name, b->name) < 0;
  });
}

inline namespace implementation {
class HashWriter : public io::WriterImpl {
 public:
  explicit HashWriter(Hasher* hasher) noexcept : hasher_(CHECK_NOTNULL(hasher)) {}

  std::size_t ideal_block_size() const noexcept { return hasher_->block_size(); }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
    if (!prologue(task, n, ptr, len)) return;
    hasher_->write(base::Bytes(ptr, len));
    task->finish(base::Result());
  }

  void close(event::Task* task, const base::Options& opts) override {
    if (!prologue(task)) return;
    hasher_->finalize();
    task->finish(base::Result());
  }

 private:
  Hasher* hasher_;
};
}  // inline namespace implementation

io::Writer Hasher::writer() {
  return io::Writer(std::make_shared<HashWriter>(this));
}

base::Result Crypter::seek(int64_t pos, int whence) {
  return base::Result::not_implemented();
}

base::Result Crypter::tell(int64_t* pos) {
  CHECK_NOTNULL(pos);
  return base::Result::not_implemented();
}

std::vector<const Hash*> all_hashes(Security min) {
  return all_impl(g_hash, min);
}

base::Result find_hash(const Hash** out, base::Chars name, Security min) {
  return find_impl(g_hash, "hash", out, name, min);
}

void register_hash(const Hash* ptr) {
  CHECK_NOTNULL(ptr);
  CHECK_NOTNULL(ptr->name);
  CHECK_NOTNULL(ptr->newfn);
  register_impl(&g_hash, ptr);
}

std::vector<const BlockCipher*> all_block_ciphers(Security min) {
  return all_impl(g_block, min);
}

base::Result find_block_cipher(const BlockCipher** out, base::Chars name,
                               Security min) {
  return find_impl(g_block, "block cipher", out, name, min);
}

void register_block_cipher(const BlockCipher* ptr) {
  CHECK_NOTNULL(ptr);
  CHECK_NOTNULL(ptr->name);
  CHECK_NOTNULL(ptr->newfn);
  register_impl(&g_block, ptr);
}

std::vector<const BlockCipherMode*> all_modes(Security min) {
  return all_impl(g_mode, min);
}

base::Result find_mode(const BlockCipherMode** out, base::Chars name,
                       Security min) {
  return find_impl(g_mode, "block cipher mode", out, name, min);
}

void register_mode(const BlockCipherMode* ptr) {
  CHECK_NOTNULL(ptr);
  CHECK_NOTNULL(ptr->name);
  CHECK_NOTNULL(ptr->newfn);
  register_impl(&g_mode, ptr);
}

std::vector<const StreamCipher*> all_stream_ciphers(Security min) {
  return all_impl(g_stream, min);
}

base::Result find_stream_cipher(const StreamCipher** out, base::Chars name,
                                Security min) {
  return find_impl(g_stream, "stream cipher", out, name, min);
}

void register_stream_cipher(const StreamCipher* ptr) {
  CHECK_NOTNULL(ptr);
  CHECK_NOTNULL(ptr->name);
  CHECK_NOTNULL(ptr->newfn);
  register_impl(&g_stream, ptr);
}

base::Result new_hash(std::unique_ptr<Hasher>* out, base::Chars name, Security min) {
  CHECK_NOTNULL(out);

  auto index = name.find(':');
  uint16_t varlen = 0;
  bool have_varlen = false;
  if (index != base::Chars::npos) {
    base::Result result;
    uint64_t x, y;
    x = 0;
    y = 8;
    if (name.substring(index, 3) == ":n=") {
      result = parse_uint64(&x, name.substring(index + 3));
    } else if (name.substring(index, 3) == ":b=") {
      result = parse_uint64(&x, name.substring(index + 3));
      y = 1;
    } else {
      result = base::Result::invalid_argument("expected \":b=<bits>\" or \":n=<bytes>\"");
    }
    if (result && x > UINT16_MAX / y) {
      result = base::Result::out_of_range("overflow");
    }
    x *= y;
    if (result && (x & 7) != 0) {
      result = base::Result::out_of_range("number of bits must be a multiple of 8");
    }
    if (!result) {
      auto msg = base::concat("failed to parse: \"", name.substring(index + 1), "\": ", result.message());
      return base::Result(result.code(), msg);
    }
    name = name.substring(0, index);
    varlen = x / 8;
    have_varlen = true;
  }

  const Hash* hash = nullptr;
  auto result = find_hash(&hash, name, min);
  if (result) {
    if (have_varlen)
      *out = hash->varfn(varlen);
    else
      *out = hash->newfn();
  }
  return result;
}

base::Result new_crypter(std::unique_ptr<Crypter>* out, base::Chars name,
                         Security min, base::Bytes key, base::Bytes iv) {
  CHECK_NOTNULL(out);

  base::Result result;
  auto index = name.find('+');
  if (index == base::Chars::npos) {
    const crypto::StreamCipher* cipher = nullptr;
    result = find_stream_cipher(&cipher, name, min);
    if (!result) return result;

    *out = cipher->newfn(key, iv);
  } else {
    auto cipher_name = name.substring(0, index);
    auto mode_name = name.substring(index + 1);

    const crypto::BlockCipher* cipher = nullptr;
    result = find_block_cipher(&cipher, cipher_name, min);
    if (!result) return result;

    const crypto::BlockCipherMode* mode = nullptr;
    result = find_mode(&mode, mode_name, min);
    if (!result) return result;

    if (streq(mode->name, "CBC") && cipher->cbcfn) {
      *out = cipher->cbcfn(key, iv);
    } else if (streq(mode->name, "CTR") && cipher->ctrfn) {
      *out = cipher->ctrfn(key, iv);
    } else {
      *out = mode->newfn(cipher->newfn(key), iv);
    }
  }
  return result;
}
}  // namespace crypto
