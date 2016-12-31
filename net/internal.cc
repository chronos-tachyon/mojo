#include "net/internal.h"

#include "base/logging.h"

static constexpr std::size_t BITS = sizeof(std::size_t) * 8;

__attribute__((const)) static std::size_t rotate(std::size_t x,
                                                 unsigned int shift) noexcept {
  return ((x >> shift) | (x << (BITS - shift)));
}

namespace net {
namespace internal {

std::size_t hash(const void* ptr, std::size_t len) noexcept {
  if (len > 0) DCHECK_NOTNULL(ptr);
  const unsigned char* p = reinterpret_cast<const unsigned char*>(ptr);
  const unsigned char* q = p + len;
  const std::size_t mul = 7907U + len * 2U;
  std::size_t h = len * 3U;
  while (p != q) {
    h = rotate(h, 27) * mul + *p;
    ++p;
  }
  return h;
}

std::size_t mix(std::size_t a, std::size_t b) noexcept {
  return rotate(a, 23) + rotate(b, BITS - 17);
}

}  // namespace internal
}  // namespace net

