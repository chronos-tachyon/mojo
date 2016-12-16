// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/handler.h"

namespace event {

namespace internal {
class FunctionHandler : public Handler {
 public:
  FunctionHandler(std::function<base::Result(Data)> f) noexcept
      : f_(std::move(f)) {}
  base::Result run(Data data) const override;

 private:
  std::function<base::Result(Data)> f_;
};

base::Result FunctionHandler::run(Data data) const { return f_(data); }
}  // namespace internal

HandlerPtr handler(std::function<base::Result(Data)> f) {
  return std::make_shared<internal::FunctionHandler>(std::move(f));
}

}  // namespace event
