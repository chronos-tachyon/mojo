#include "base/endian.h"

#include <cstring>
#include <limits>

#include "base/logging.h"

namespace base {

namespace {

struct BigEndian : public Endian {
  BigEndian() noexcept = default;

  uint16_t get_u16(const char* buf) const noexcept override {
    CHECK_NOTNULL(buf);
    const auto* p = reinterpret_cast<const unsigned char*>(buf);
    using W = uint16_t;
    return (W(p[0]) << 8) | W(p[1]);
  }

  uint32_t get_u32(const char* buf) const noexcept override {
    CHECK_NOTNULL(buf);
    const auto* p = reinterpret_cast<const unsigned char*>(buf);
    using W = uint32_t;
    return (W(p[0]) << 24) | (W(p[1]) << 16) | (W(p[2]) << 8) | W(p[3]);
  }

  uint64_t get_u64(const char* buf) const noexcept override {
    CHECK_NOTNULL(buf);
    const auto* p = reinterpret_cast<const unsigned char*>(buf);
    using W = uint64_t;
    return (W(p[0]) << 56) | (W(p[1]) << 48) | (W(p[2]) << 40) |
           (W(p[3]) << 32) | (W(p[4]) << 24) | (W(p[5]) << 16) |
           (W(p[6]) << 8) | W(p[7]);
  }

  void put_u16(char* buf, uint16_t in) const noexcept override {
    CHECK_NOTNULL(buf);
    auto* p = reinterpret_cast<unsigned char*>(buf);
    p[0] = (in >> 8) & 0xff;
    p[1] = in & 0xff;
  }

  void put_u32(char* buf, uint32_t in) const noexcept override {
    CHECK_NOTNULL(buf);
    auto* p = reinterpret_cast<unsigned char*>(buf);
    p[0] = (in >> 24) & 0xff;
    p[1] = (in >> 16) & 0xff;
    p[2] = (in >> 8) & 0xff;
    p[3] = in & 0xff;
  }

  void put_u64(char* buf, uint64_t in) const noexcept override {
    CHECK_NOTNULL(buf);
    auto* p = reinterpret_cast<unsigned char*>(buf);
    p[0] = (in >> 56) & 0xff;
    p[1] = (in >> 48) & 0xff;
    p[2] = (in >> 40) & 0xff;
    p[3] = (in >> 32) & 0xff;
    p[4] = (in >> 24) & 0xff;
    p[5] = (in >> 16) & 0xff;
    p[6] = (in >> 8) & 0xff;
    p[7] = in & 0xff;
  }
};

struct LittleEndian : public Endian {
  LittleEndian() noexcept = default;

  uint16_t get_u16(const char* buf) const noexcept override {
    CHECK_NOTNULL(buf);
    const auto* p = reinterpret_cast<const unsigned char*>(buf);
    using W = uint16_t;
    return W(p[0]) | (W(p[1]) << 8);
  }

  uint32_t get_u32(const char* buf) const noexcept override {
    CHECK_NOTNULL(buf);
    const auto* p = reinterpret_cast<const unsigned char*>(buf);
    using W = uint32_t;
    return W(p[0]) | (W(p[1]) << 8) | (W(p[2]) << 16) | (W(p[3]) << 24);
  }

  uint64_t get_u64(const char* buf) const noexcept override {
    CHECK_NOTNULL(buf);
    const auto* p = reinterpret_cast<const unsigned char*>(buf);
    using W = uint64_t;
    return W(p[0]) | (W(p[1]) << 8) | (W(p[2]) << 16) | (W(p[3]) << 24) |
           (W(p[4]) << 32) | (W(p[5]) << 40) | (W(p[6]) << 48) |
           (W(p[7]) << 56);
  }

  void put_u16(char* buf, uint16_t in) const noexcept override {
    CHECK_NOTNULL(buf);
    auto* p = reinterpret_cast<unsigned char*>(buf);
    p[0] = in & 0xff;
    p[1] = (in >> 8) & 0xff;
  }

  void put_u32(char* buf, uint32_t in) const noexcept override {
    CHECK_NOTNULL(buf);
    auto* p = reinterpret_cast<unsigned char*>(buf);
    p[0] = in & 0xff;
    p[1] = (in >> 8) & 0xff;
    p[2] = (in >> 16) & 0xff;
    p[3] = (in >> 24) & 0xff;
  }

  void put_u64(char* buf, uint64_t in) const noexcept override {
    CHECK_NOTNULL(buf);
    auto* p = reinterpret_cast<unsigned char*>(buf);
    p[0] = in & 0xff;
    p[1] = (in >> 8) & 0xff;
    p[2] = (in >> 16) & 0xff;
    p[3] = (in >> 24) & 0xff;
    p[4] = (in >> 32) & 0xff;
    p[5] = (in >> 40) & 0xff;
    p[6] = (in >> 48) & 0xff;
    p[7] = (in >> 56) & 0xff;
  }
};

struct NativeEndian : public Endian {
  NativeEndian() noexcept = default;

  static_assert(sizeof(uint16_t) == 2, "");
  static_assert(sizeof(uint32_t) == 4, "");
  static_assert(sizeof(uint64_t) == 8, "");

  uint16_t get_u16(const char* buf) const noexcept override {
    uint16_t tmp;
    CHECK_NOTNULL(buf);
    ::memcpy(&tmp, buf, 2);
    return tmp;
  }

  uint32_t get_u32(const char* buf) const noexcept override {
    uint32_t tmp;
    CHECK_NOTNULL(buf);
    ::memcpy(&tmp, buf, 4);
    return tmp;
  }

  uint64_t get_u64(const char* buf) const noexcept override {
    uint64_t tmp;
    CHECK_NOTNULL(buf);
    ::memcpy(&tmp, buf, 8);
    return tmp;
  }

  void put_u16(char* buf, uint16_t in) const noexcept override {
    CHECK_NOTNULL(buf);
    ::memcpy(buf, &in, 2);
  }

  void put_u32(char* buf, uint32_t in) const noexcept override {
    CHECK_NOTNULL(buf);
    ::memcpy(buf, &in, 4);
  }

  void put_u64(char* buf, uint64_t in) const noexcept override {
    CHECK_NOTNULL(buf);
    ::memcpy(buf, &in, 8);
  }
};

static const BigEndian kBE = {};
static const LittleEndian kLE = {};
static const NativeEndian kNE = {};

}  // anonymous namespace

const Endian* const kBigEndian = &kBE;
const Endian* const kLittleEndian = &kLE;
const Endian* const kNativeEndian = &kNE;

}  // namespace base
