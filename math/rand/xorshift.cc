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
// 64-bit variant of xorshift*
//
// Algorithm and constants cribbed from:
//  https://en.wikipedia.org/wiki/Xorshift#xorshift.2A
//
class XorShift : public Source {
 public:
  static constexpr unsigned int A = 12;
  static constexpr unsigned int B = 25;
  static constexpr unsigned int C = 27;
  static constexpr uint64_t M = 0x2545f4914f6cdd1dULL;

  XorShift(const XorShift&) noexcept = default;
  XorShift(XorShift&&) noexcept = default;

  XorShift(uint64_t seedval) noexcept {
    seed(seedval);
  }

  std::unique_ptr<Source> copy() const override {
    return make_unique<XorShift>(*this);
  }

  void seed(uint64_t seedval) noexcept override {
    if (!seedval) seedval = 1;
    state_ = seedval;
  }

  uint64_t next() noexcept override {
    uint64_t x = state_;
    x ^= (x >> A);
    x ^= (x << B);
    x ^= (x >> C);
    state_ = x;
    return x * M;
  }

 private:
  uint64_t state_;
};
}  // inline namespace implementation

SourcePtr new_xorshift_source(uint64_t seed) {
  return make_unique<XorShift>(seed);
}

SourcePtr new_xorshift_source() { return new_xorshift_source(default_seed()); }

}  // namespace rand
}  // namespace math
