// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "math/rand/rand.h"

template <typename T, typename... Args>
static std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

namespace math {
namespace rand {
inline namespace implementation {
// Linear Congruential Generator
//
// Constants cribbed from:
//  https://en.wikipedia.org/wiki/Linear_congruential_generator#Parameters_in_common_use
//  (specifically, POSIX [ln]rand48)
//
// Actual values are not compatible with nrand48(3): we want 64 bits of output,
// so we run it twice per invocation and glue the outputs together.  We also
// seed with a 64-bit value, not a 32-bit value concatenated with 0x330e.
//
class LinearCongruential : public Source {
 public:
  static constexpr uint64_t A = 0x5deece66dULL;
  static constexpr uint64_t C = 0xbULL;
  static constexpr uint64_t M_SUB_1 = 0xffffffffffffULL;  // 48 bits
  static constexpr uint64_t FILTER = 0xffffffff0000ULL;   // bits 16..47

  LinearCongruential(const LinearCongruential&) noexcept = default;
  LinearCongruential(LinearCongruential&&) noexcept = default;

  LinearCongruential(uint64_t seedval) noexcept { seed(seedval); }

  std::unique_ptr<Source> copy() const override {
    return make_unique<LinearCongruential>(*this);
  }

  void seed(uint64_t seedval) noexcept override { state_ = seedval; }

  uint64_t next() noexcept override {
    auto x = ((state_ * A) + C) & M_SUB_1;
    auto y = ((x * A) + C) & M_SUB_1;
    auto value = ((x & FILTER) << 16) | ((y & FILTER) >> 16);
    state_ = y;
    return value;
  }

 private:
  uint64_t state_;
};
}  // inline namespace implementation

SourcePtr new_lcg_source(uint64_t seed) {
  return make_unique<LinearCongruential>(seed);
}

SourcePtr new_lcg_source() { return new_lcg_source(default_seed()); }

}  // namespace rand
}  // namespace math
