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
// Linear Feedback Shift Register
//
// Basic algorithm cribbed from:
//  https://en.wikipedia.org/wiki/Linear-feedback_shift_register#Fibonacci_LFSRs
//
// Taps cribbed from:
//  http://www.xilinx.com/support/documentation/application_notes/xapp052.pdf
//  Page 5 (n=64, taps=64,63,61,60)
//
class LinearFeedbackShiftRegister : public Source {
 public:
  LinearFeedbackShiftRegister(const LinearFeedbackShiftRegister&) noexcept = default;
  LinearFeedbackShiftRegister(LinearFeedbackShiftRegister&&) noexcept = default;

  LinearFeedbackShiftRegister(uint64_t seedval) noexcept {
    seed(seedval);
  }

  std::unique_ptr<Source> copy() const override {
    return make_unique<LinearFeedbackShiftRegister>(*this);
  }

  void seed(uint64_t seedval) noexcept override {
    if (!seedval) seedval = 1;
    state_ = seedval;
    next();
  }

  uint64_t next() noexcept override {
    uint64_t value = 0;
    for (std::size_t i = 0; i < 64; ++i) {
      uint8_t tap0 = (state_ >> 63);
      value = (value << 1) | tap0;
      uint8_t tap1 = (state_ >> 62);
      uint8_t tap2 = (state_ >> 60);
      uint8_t tap3 = (state_ >> 59);
      uint8_t bit = (tap0 ^ tap1 ^ tap2 ^ tap3) & 1;
      state_ = (state_ << 1) | bit;
    }
    return value;
  }

 private:
  uint64_t state_;
};
}  // inline namespace implementation

SourcePtr new_lfsr_source(uint64_t seedval) {
  return make_unique<LinearFeedbackShiftRegister>(seedval);
}

SourcePtr new_lfsr_source() { return new_lfsr_source(default_seed()); }

}  // namespace rand
}  // namespace math
