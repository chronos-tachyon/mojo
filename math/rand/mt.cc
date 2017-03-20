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
// Mersenne Twister (specifically: MT19937-64)
//
// Cribbed from:
//  https://en.wikipedia.org/wiki/Mersenne_Twister#C.23_implementation
//
class MersenneTwister : public Source {
 public:
  static constexpr unsigned int W = 64;
  static constexpr unsigned int N = 312;
  static constexpr unsigned int M = 156;
  static constexpr unsigned int R = 31;
  static constexpr unsigned int U = 29;
  static constexpr unsigned int S = 17;
  static constexpr unsigned int T = 37;
  static constexpr unsigned int L = 43;

  static constexpr uint64_t A = 0xb5026f5aa96619e9ULL;
  static constexpr uint64_t D = 0x5555555555555555ULL;
  static constexpr uint64_t B = 0x71d67fffeda60000ULL;
  static constexpr uint64_t C = 0xfff7eee000000000ULL;
  static constexpr uint64_t F = 6364136223846793005ULL;

  static constexpr uint64_t LO_MASK = 0x7fffffffULL;
  static constexpr uint64_t HI_MASK = ~LO_MASK;

  MersenneTwister(const MersenneTwister&) noexcept = default;
  MersenneTwister(MersenneTwister&&) noexcept = default;

  MersenneTwister(uint64_t seedval) noexcept {
    seed(seedval);
  }

  std::unique_ptr<Source> copy() const override {
    return make_unique<MersenneTwister>(*this);
  }

  void seed(uint64_t seedval) noexcept override {
    index_ = N;
    mt_[0] = seedval;
    for (std::size_t i = 1; i < N; ++i) {
      mt_[i] = (F * (mt_[i - 1] ^ (mt_[i - 1] >> (W - 2))) + i);
    }
  }

  uint64_t next() noexcept override {
    if (index_ >= N) twist();
    uint64_t y = mt_[index_];
    y = y ^ ((y >> U) & D);
    y = y ^ ((y >> S) & B);
    y = y ^ ((y >> T) & C);
    y = y ^ (y >> L);
    ++index_;
    return y;
  }

 private:
  void twist() noexcept {
    for (std::size_t i = 0; i < N; ++i) {
      uint64_t x = (mt_[i] & HI_MASK) + (mt_[(i + 1) % N] & LO_MASK);
      uint64_t xa = (x >> 1);
      if (x & 1) xa ^= A;
      mt_[i] = mt_[(i + M) % N] ^ xa;
    }
    index_ = 0;
  }

  uint64_t mt_[N];
  uint64_t index_;
};
}  // inline namespace implementation

SourcePtr new_mt_source(uint64_t seed) {
  return make_unique<MersenneTwister>(seed);
}

SourcePtr new_mt_source() { return new_mt_source(default_seed()); }

}  // namespace rand
}  // namespace math
