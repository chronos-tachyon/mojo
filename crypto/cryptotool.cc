// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include <unistd.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "base/bytes.h"
#include "base/chars.h"
#include "base/concat.h"
#include "base/flag.h"
#include "base/logging.h"
#include "base/result.h"
#include "crypto/crypto.h"
#include "crypto/security.h"
#include "encoding/base64.h"
#include "encoding/hex.h"

template <typename... Args>
static __attribute__((noreturn)) void die(Args&&... args) {
  std::cerr << "ERROR: " << base::concat(std::forward<Args>(args)...)
            << std::endl;
  exit(2);
}

static std::vector<uint8_t> make_buffer(std::size_t block_size) {
  std::vector<uint8_t> buf;
  buf.resize(((block_size + 4095) / block_size) * block_size);
  return buf;
}

static std::vector<uint8_t> decode_flag(base::FlagSet* flags,
                                        base::Chars flag) {
  std::vector<uint8_t> out;
  base::Chars text = flags->get_string(flag)->value();
  if (text.remove_prefix("hex:")) {
    out.resize(decoded_length(encoding::HEX, text.size()));
    auto decoded = decode_to(encoding::HEX, out, text);
    if (!decoded.first) {
      die("failed to decode --", flag, " as hexadecimal data");
    }
    out.resize(decoded.second);
  } else if (text.remove_prefix("base64:")) {
    out.resize(decoded_length(encoding::BASE64, text.size()));
    auto decoded = decode_to(encoding::BASE64, out, text);
    if (!decoded.first) {
      die("failed to decode --", flag, " as base-64 data");
    }
    out.resize(decoded.second);
  } else {
    base::Bytes data = text;
    out = data.as_vector();
  }
  return out;
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
    if (value * 10 > UINT64_MAX - x)
      return base::Result::out_of_range("overflow");
    value = (value * 10) + x;
  }
  *out = value;
  return base::Result();
}

static std::size_t do_read(std::vector<uint8_t>* buf, int fd,
                           const char* path) {
redo:
  ssize_t n = ::read(fd, buf->data(), buf->size());
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    auto result = base::Result::from_errno(err_no, "read(2) path=", path);
    LOG(FATAL) << result;
  }
  return n;
}

static void do_write(base::Bytes buf, int fd, const char* path) {
  while (!buf.empty()) {
    ssize_t n = ::write(fd, buf.data(), buf.size());
    if (n < 0) {
      int err_no = errno;
      if (err_no == EINTR) continue;
      auto result = base::Result::from_errno(err_no, "write(2) path=", path);
      LOG(FATAL) << result;
    }
    buf.remove_prefix(n);
  }
}

static std::string fmt_size(uint16_t x) {
  return base::concat((x * 8), " bits (", x, " bytes)");
}

static std::string fmt_flags(uint8_t x, const char* a = nullptr,
                             const char* b = nullptr, const char* c = nullptr,
                             const char* d = nullptr, const char* e = nullptr,
                             const char* f = nullptr, const char* g = nullptr,
                             const char* h = nullptr) {
  std::string out;
  if (x & 0x01) {
    out.append(a ? a : "01");
    out.push_back(' ');
  }
  if (x & 0x02) {
    out.append(b ? b : "02");
    out.push_back(' ');
  }
  if (x & 0x04) {
    out.append(c ? c : "04");
    out.push_back(' ');
  }
  if (x & 0x08) {
    out.append(d ? d : "08");
    out.push_back(' ');
  }
  if (x & 0x10) {
    out.append(e ? e : "10");
    out.push_back(' ');
  }
  if (x & 0x20) {
    out.append(f ? f : "20");
    out.push_back(' ');
  }
  if (x & 0x40) {
    out.append(g ? g : "40");
    out.push_back(' ');
  }
  if (x & 0x80) {
    out.append(h ? h : "80");
    out.push_back(' ');
  }
  if (out.empty()) out.append("none ");
  out.pop_back();
  return out;
}

static void cmd_list(base::FlagSet* flags, crypto::Security security) {
  std::cout << "HASH ALGORITHMS\n"
            << "---------------\n";
  for (const auto* hash : crypto::all_hashes(security)) {
    std::cout << "\n"
              << "Name       : " << hash->name << "\n"
              << "Block Size : " << fmt_size(hash->block_size) << "\n"
              << "Output Size: " << fmt_size(hash->output_size) << "\n"
              << "Security   : " << security_name(hash->security) << "\n"
              << "Flags      : " << fmt_flags(hash->flags, "varlen", "sponge")
              << "\n";
  }
  std::cout << "\n"
            << "BLOCK CIPHERS\n"
            << "-------------\n";
  for (const auto* block : crypto::all_block_ciphers(security)) {
    std::cout << "\n"
              << "Name       : " << block->name << "\n"
              << "Block Size : " << fmt_size(block->block_size) << "\n"
              << "Key Size   : " << fmt_size(block->key_size) << "\n"
              << "Security   : " << security_name(block->security) << "\n"
              << "Flags      : " << fmt_flags(block->flags) << "\n";
  }
  std::cout << "\n"
            << "BLOCK CIPHER MODES\n"
            << "------------------\n";
  for (const auto* mode : crypto::all_modes(security)) {
    std::cout << "\n"
              << "Name       : " << mode->name << "\n"
              << "IV Size    : " << mode->iv_size << " × block size\n"
              << "Security   : " << security_name(mode->security) << "\n"
              << "Flags      : "
              << fmt_flags(mode->flags, "seekable", "streaming") << "\n";
  }
  std::cout << "\n"
            << "STREAM CIPHERS\n"
            << "--------------\n";
  for (const auto* stream : crypto::all_stream_ciphers(security)) {
    std::cout << "\n"
              << "Name       : " << stream->name << "\n"
              << "Block Size : " << fmt_size(stream->block_size) << "\n"
              << "Key Size   : " << fmt_size(stream->key_size) << "\n"
              << "Nonce Size : " << fmt_size(stream->nonce_size) << "\n"
              << "Security   : " << security_name(stream->security) << "\n"
              << "Flags      : " << fmt_flags(stream->flags, "seekable")
              << "\n";
  }
  std::cout << std::endl;
}

static void cmd_hash(base::FlagSet* flags, crypto::Security security) {
  base::Chars name = flags->get_string("hash")->value();
  std::unique_ptr<crypto::Hasher> hasher;
  auto result = crypto::new_hash(&hasher, name, security);
  if (!result) die(result);

  auto buf = make_buffer(hasher->block_size());
  while (true) {
    auto n = do_read(&buf, 0, "/dev/stdin");
    if (n == 0) break;
    hasher->write(base::Bytes(buf.data(), n));
  }
  hasher->finalize();
  buf.resize(hasher->output_size());
  hasher->sum(buf);
  do_write(buf, 1, "/dev/stdout");
}

static void encrypt_or_decrypt(base::FlagSet* flags, crypto::Security security,
                               bool do_encrypt) {
  base::Chars name = flags->get_string("cipher")->value();
  auto key = decode_flag(flags, "key");
  auto iv = decode_flag(flags, "iv");

  std::unique_ptr<crypto::Crypter> crypter;
  auto result = crypto::new_crypter(&crypter, name, security, key, iv);
  if (!result) die(result);

  auto* offset = flags->get_string("offset");
  if (offset->is_set()) {
    uint64_t n = 0;
    auto result = parse_uint64(&n, offset->value());
    if (!result) die("invalid value for --offset: ", result);
    result = crypter->seek(n, SEEK_SET);
    if (!result) die(result);
  }

  auto buf = make_buffer(crypter->block_size());
  while (true) {
    auto n = do_read(&buf, 0, "/dev/stdin");
    if (n == 0) break;
    base::MutableBytes mut(buf.data(), n);
    if (do_encrypt)
      crypter->encrypt(mut, mut);
    else
      crypter->decrypt(mut, mut);
    do_write(mut, 1, "/dev/stdout");
  }
}

static void cmd_encrypt(base::FlagSet* flags, crypto::Security security) {
  encrypt_or_decrypt(flags, security, true);
}

static void cmd_decrypt(base::FlagSet* flags, crypto::Security security) {
  encrypt_or_decrypt(flags, security, false);
}

int main(int argc, char** argv) {
  base::FlagSet flags;
  flags.set_description(
      "Driver for testing cryptographic cipher implementations");
  flags.add_help();
  flags.add_version();
  flags
      .add_choice("cmd", {"list", "hash", "encrypt", "decrypt"}, "",
                  "Action to perform")
      .mark_required();
  flags.add_choice(
      "security", {"strong", "secure", "weak", "broken"}, "secure",
      "Selects the minimum security level which all algorithms must meet");
  flags.add_string("hash", "", "Hash algorithm to use");
  flags.add_string("cipher", "",
                   "Stream cipher, or block cipher + mode, to use");
  flags.add_string("key", "", "Key to use (hex)");
  flags.add_string("iv", "", "Initialization Vector or Nonce to use (hex)")
      .add_alias("nonce");
  flags.add_string("offset", "0",
                   "Position within the stream (seekable ciphers only)");
  flags.parse(argc, argv);
  if (!flags.args().empty()) {
    flags.show_help(std::cerr);
    die("unexpected positional arguments");
  }

  crypto::Security security;
  base::Chars name = flags.get_choice("security")->value();
  if (name == "strong") {
    security = crypto::Security::strong;
  } else if (name == "secure") {
    security = crypto::Security::secure;
  } else if (name == "weak") {
    security = crypto::Security::weak;
  } else if (name == "broken") {
    security = crypto::Security::broken;
  } else {
    die("invalid --security value");
  }

  void (*cmd)(base::FlagSet*, crypto::Security);
  name = flags.get_choice("cmd")->value();
  if (name == "list") {
    cmd = cmd_list;
  } else if (name == "hash") {
    cmd = cmd_hash;
  } else if (name == "encrypt") {
    cmd = cmd_encrypt;
  } else if (name == "decrypt") {
    cmd = cmd_decrypt;
  } else {
    die("invalid --cmd value");
  }
  cmd(&flags, security);
  return 0;
}
