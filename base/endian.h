#ifndef BASE_ENDIAN_H
#define BASE_ENDIAN_H

#include <cstdint>

namespace base {

struct Endian {
  virtual ~Endian() noexcept = default;

  virtual uint16_t get_u16(const char* buf) const noexcept = 0;
  virtual uint32_t get_u32(const char* buf) const noexcept = 0;
  virtual uint64_t get_u64(const char* buf) const noexcept = 0;

  virtual void put_u16(char* buf, uint16_t in) const noexcept = 0;
  virtual void put_u32(char* buf, uint32_t in) const noexcept = 0;
  virtual void put_u64(char* buf, uint64_t in) const noexcept = 0;
};

extern const Endian* const kBigEndian;
extern const Endian* const kLittleEndian;
extern const Endian* const kNativeEndian;

}  // namespace base

#endif  // BASE_ENDIAN_H
