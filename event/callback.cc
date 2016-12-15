// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/callback.h"

namespace event {

namespace internal {
class FunctionCallback : public Callback {
 public:
  FunctionCallback(std::function<base::Result()> f) noexcept
      : f_(std::move(f)) {}
  base::Result run() override;

 private:
  std::function<base::Result()> f_;
};

base::Result FunctionCallback::run() {
  std::function<base::Result()> f = std::move(f_);
  return f();
}
}  // namespace internal

CallbackPtr callback(std::function<base::Result()> f) {
  return CallbackPtr(new internal::FunctionCallback(std::move(f)));
}

}  // namespace event
