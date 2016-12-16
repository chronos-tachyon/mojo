// event/handler.h - Defines user-specified repeatable event handler functions
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <algorithm>
#include <memory>

#include "base/backport.h"
#include "base/result.h"
#include "event/callback.h"
#include "event/data.h"

namespace event {

// A Handler is a closure of captured function context which may be resumed in
// response to incoming events, possibly on another thread from that of the
// Handler's creator.
//
// - Handler objects are normally passed around wrapped in std::shared_ptr.
// - Handlers may be called any number of times.
// - Handlers may be called concurrently from multiple threads.
//
// THREAD SAFETY: Handlers MUST be thread-safe.
//
class Handler {
 protected:
  Handler() noexcept = default;

 public:
  // Handlers are neither copyable nor moveable.
  Handler(const Handler&) = delete;
  Handler(Handler&&) = delete;
  Handler& operator=(const Handler&) = delete;
  Handler& operator=(Handler&&) = delete;

  virtual ~Handler() noexcept = default;

  // Invokes the handler with the given event data.
  // MAY be called any number of times.
  virtual base::Result run(Data data) const = 0;
};

using HandlerPtr = std::shared_ptr<Handler>;

// Implementation details {{{

namespace internal {
template <typename Function, typename... Args>
class ClosureHandler : public Handler {
 public:
  ClosureHandler(Function f, std::tuple<Args...> t) noexcept
      : f_(std::move(f)),
        t_(std::move(t)) {}

  base::Result run(Data data) const override {
    return run(base::backport::make_index_sequence<sizeof...(Args)>(), data);
  }

 private:
  template <std::size_t... Ns>
  base::Result run(base::backport::index_sequence<Ns...>, Data data) const {
    return f_(std::get<Ns>(t_)..., data);
  }

  Function f_;
  std::tuple<Args...> t_;
};
}  // namespace internal

// }}}

// Constructs a Handler from an existing function object.
HandlerPtr handler(std::function<base::Result(Data)> f);

// Constructs a Handler from the given function/functor and arguments.
template <typename Function, typename... Args>
HandlerPtr handler(Function&& f, Args&&... args) {
  using T =
      internal::ClosureHandler<typename std::remove_reference<Function>::type,
                               typename std::remove_reference<Args>::type...>;
  return std::make_shared<T>(
      std::forward<Function>(f),
      std::forward_as_tuple<Args...>(std::forward<Args>(args)...));
}

}  // namespace event

#endif  // EVENT_HANDLER_H
