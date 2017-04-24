// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "crypto/subtle.h"

#include <sys/mman.h>
#include <unistd.h>

#include <new>
#include <stdexcept>
#include <system_error>

#include "base/logging.h"

namespace crypto {
namespace subtle {

bool consttime_eq(const uint8_t* a, const uint8_t* b,
                  std::size_t len) noexcept {
  unsigned int result = 0;
  for (std::size_t i = 0; i < len; ++i) {
    result |= (a[i] ^ b[i]);
  }
  return (result == 0);
}

static std::size_t page_size() { return ::sysconf(_SC_PAGE_SIZE); }

static std::size_t pad_to_page_size(std::size_t len) {
  std::size_t x = page_size();
  std::size_t y = (x - 1);
  if (len > SIZE_MAX - y) throw std::overflow_error("allocation overflow");
  return (len + y) & ~y;
}

void* secure_allocate(std::size_t len) {
  len = pad_to_page_size(len);
  void* ptr = ::mmap(nullptr, len, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
  if (ptr == MAP_FAILED) {
    int err_no = errno;
    throw std::system_error(err_no, std::system_category(), "mmap(2)");
  }
  int rc = ::mlock(ptr, len);
  if (rc != 0) {
    int err_no = errno;
    throw std::system_error(err_no, std::system_category(), "mlock(2)");
  }
  return ptr;
}

void secure_deallocate(void* ptr, std::size_t len) {
  len = pad_to_page_size(len);
  ::bzero(ptr, len);
  int rc = ::munmap(ptr, len);
  if (rc != 0) {
    int err_no = errno;
    throw std::system_error(err_no, std::system_category(), "munmap(2)");
  }
}

}  // namespace subtle
}  // namespace crypto
