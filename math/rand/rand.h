// math/rand/rand.h - Interface for non-secure PRNGs
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef MATH_RAND_RAND_H
#define MATH_RAND_RAND_H

#include <cstdint>
#include <memory>

namespace math {
namespace rand {

class Source {
 protected:
  Source() noexcept = default;
  Source(const Source&) noexcept = default;
  Source(Source&&) noexcept = default;
  Source& operator=(const Source&) noexcept = default;
  Source& operator=(Source&&) noexcept = default;

 public:
  virtual ~Source() noexcept = default;
  virtual std::unique_ptr<Source> copy() const = 0;
  virtual void seed(uint64_t n) noexcept = 0;
  virtual uint64_t next() noexcept = 0;
};

using SourcePtr = std::unique_ptr<Source>;

uint64_t default_seed();
SourcePtr new_lcg_source(uint64_t seed);
SourcePtr new_lcg_source();
SourcePtr new_lfsr_source(uint64_t seed);
SourcePtr new_lfsr_source();
SourcePtr new_mt_source(uint64_t seed);
SourcePtr new_mt_source();
SourcePtr new_xorshift_source(uint64_t seed);
SourcePtr new_xorshift_source();
SourcePtr new_default_source();

class Random {
 public:
  Random(SourcePtr ptr) noexcept : ptr_(std::move(ptr)), val_(0), len_(0) {}

  Random() : Random(new_default_source()) {}

  Random(const Random& other)
      : ptr_(copy(other.ptr_)), val_(other.val_), len_(other.len_) {}
  Random& operator=(const Random& other) {
    ptr_ = copy(other.ptr_);
    val_ = other.val_;
    len_ = other.len_;
    return *this;
  }

  Random(Random&&) noexcept = default;
  Random& operator=(Random&&) noexcept = default;

  void assert_valid() noexcept;
  const SourcePtr& implementation() const noexcept { return ptr_; }
  SourcePtr& implementation() noexcept { return ptr_; }

  void seed(uint64_t n) noexcept {
    assert_valid();
    ptr_->seed(n);
  }

  bool uniform_bit() noexcept;
  uint8_t uniform_u8() noexcept;
  uint16_t uniform_u16() noexcept;
  uint32_t uniform_u24() noexcept;
  uint32_t uniform_u32() noexcept;
  uint64_t uniform_u48() noexcept;
  uint64_t uniform_u64() noexcept {
    assert_valid();
    return ptr_->next();
  }

  int8_t uniform_s7() noexcept { return uniform_u8() >> 1; }
  int16_t uniform_s15() noexcept { return uniform_u16() >> 1; }
  int32_t uniform_s23() noexcept { return uniform_u24() >> 1; }
  int32_t uniform_s31() noexcept { return uniform_u32() >> 1; }
  int64_t uniform_s47() noexcept { return uniform_u48() >> 1; }
  int64_t uniform_s63() noexcept { return uniform_u64() >> 1; }

  // Implementation of UniformRandomBitGenerator concept {{{
  using result_type = uint64_t;
  result_type min() const noexcept { return 0ULL; }
  result_type max() const noexcept { return 0xffffffffffffffffULL; }
  result_type operator()() noexcept { return uniform_u64(); }
  // }}}

 private:
  static SourcePtr copy(const SourcePtr& ptr) {
    if (ptr)
      return ptr->copy();
    else
      return nullptr;
  }

  SourcePtr ptr_;
  uint64_t val_;  // Holds the unused bits from the last |ptr_->next()|
  uint8_t len_;   // Holds the number of bits in |val_| that are not yet used
};

}  // namespace rand
}  // namespace math

#endif  // MATH_RAND_RAND_H
