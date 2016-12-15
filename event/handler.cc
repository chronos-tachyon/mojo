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

class HandlerCallback : public Callback {
 public:
  HandlerCallback(std::weak_ptr<Handler> handler, Data data)
      : handler_(std::move(handler)), data_(std::move(data)) {}
  base::Result run() override;

 private:
  std::weak_ptr<Handler> handler_;
  Data data_;
};

base::Result HandlerCallback::run() {
  auto strong = handler_.lock();
  if (strong) return strong->run(data_);
  return base::Result::cancelled();
}
}  // namespace internal

HandlerPtr handler(std::function<base::Result(Data)> f) {
  return std::make_shared<internal::FunctionHandler>(std::move(f));
}

CallbackPtr handler_callback(std::weak_ptr<Handler> h, Data d) {
  return CallbackPtr(new internal::HandlerCallback(std::move(h), std::move(d)));
}

}  // namespace event
