// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "math/rand/rand.h"

#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <system_error>

#include "base/logging.h"

namespace math {
namespace rand {

void Random::assert_valid() noexcept {
  CHECK_NOTNULL(ptr_.get());
}

bool Random::uniform_bit() noexcept {
  if (len_ < 1) {
    assert_valid();
    val_ = ptr_->next();
    len_ = 64;
  }
  bool value = (val_ & 1) != 0;
  val_ >>= 1;
  --len_;
  return value;
}

uint8_t Random::uniform_u8() noexcept {
  if (len_ < 8) {
    assert_valid();
    val_ = ptr_->next();
    len_ = 64;
  }
  uint8_t value = (val_ & ((1ULL << 8) - 1));
  val_ >>= 8;
  len_ -= 8;
  return value;
}

uint16_t Random::uniform_u16() noexcept {
  if (len_ < 16) {
    assert_valid();
    val_ = ptr_->next();
    len_ = 64;
  }
  uint16_t value = (val_ & ((1ULL << 16) - 1));
  val_ >>= 16;
  len_ -= 16;
  return value;
}

uint32_t Random::uniform_u24() noexcept {
  if (len_ < 24) {
    assert_valid();
    val_ = ptr_->next();
    len_ = 64;
  }
  uint32_t value = (val_ & ((1ULL << 24) - 1));
  val_ >>= 24;
  len_ -= 24;
  return value;
}

uint32_t Random::uniform_u32() noexcept {
  if (len_ < 32) {
    assert_valid();
    val_ = ptr_->next();
    len_ = 64;
  }
  uint32_t value = (val_ & ((1ULL << 32) - 1));
  val_ >>= 32;
  len_ -= 32;
  return value;
}

uint64_t Random::uniform_u48() noexcept {
  if (len_ < 48) {
    assert_valid();
    val_ = ptr_->next();
    len_ = 64;
  }
  uint64_t value = (val_ & ((1ULL << 48) - 1));
  val_ >>= 48;
  len_ -= 48;
  return value;
}

static uint64_t make_default_seed() {
  const char* envvar = ::getenv("TEST_RANDOM_SEED");
  if (envvar) {
    const char* end = envvar;
    auto value = ::strtoull(envvar, const_cast<char**>(&end), 0);
    if (!*end) return value;
  }

  struct timespec ts;
  ::bzero(&ts, sizeof(ts));

  int rc = clock_gettime(CLOCK_REALTIME, &ts);
  if (rc != 0) {
    int err_no = errno;
    throw std::system_error(err_no, std::system_category(), "clock_gettime(2)");
  }

  uint64_t x;
  if (ts.tv_sec >= 0)
    x = uint64_t(ts.tv_sec);
  else
    x = ~uint64_t(-(ts.tv_sec + 1));
  x ^= ::getpid();
  x <<= 32;
  x |= uint64_t(uint32_t(ts.tv_nsec));
  return x;
}

uint64_t default_seed() {
  static uint64_t seed = make_default_seed();
  return seed;
}

SourcePtr new_default_source() { return new_xorshift_source(); }

}  // namespace rand
}  // namespace math
