// event/callback.h - Defines user-specified oneshot callback functions
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef EVENT_CALLBACK_H
#define EVENT_CALLBACK_H

#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>

#include "base/backport.h"
#include "base/result.h"

namespace event {

// A Callback is a closure of captured function context which may be resumed,
// possibly on another thread from that of the Callback's creator.
//
// - Callback objects are normally passed around wrapped in std::unique_ptr.
// - Callbacks are NEVER invoked more than once.
//
class Callback {
 protected:
  Callback() noexcept = default;

 public:
  // Callbacks are neither copyable nor moveable.
  Callback(const Callback&) = delete;
  Callback(Callback&&) = delete;
  Callback& operator=(const Callback&) = delete;
  Callback& operator=(Callback&&) = delete;

  virtual ~Callback() noexcept = default;

  // Invokes the callback.
  // MUST be called either 0 or 1 times.
  virtual base::Result run() = 0;
};

// Implementation details {{{

namespace internal {
template <typename Function, typename... Args>
class ClosureCallback : public Callback {
 public:
  ClosureCallback(Function f, std::tuple<Args...> t) noexcept
      : f_(std::move(f)),
        t_(std::move(t)) {}

  base::Result run() override {
    return run(base::backport::make_index_sequence<sizeof...(Args)>());
  }

 private:
  template <std::size_t... Ns>
  base::Result run(base::backport::index_sequence<Ns...>) {
    return f_(std::move(std::get<Ns>(t_))...);
  }

  Function f_;
  std::tuple<Args...> t_;
};
}  // namespace internal

// }}}

// Constructs a Callback from an existing std::function object.
std::unique_ptr<Callback> callback(std::function<base::Result()> f);

// Constructs a Callback from the given function/functor and arguments.
template <typename Function, typename... Args>
std::unique_ptr<Callback> callback(Function&& f, Args&&... args) {
  using T =
      internal::ClosureCallback<typename std::remove_reference<Function>::type,
                                typename std::remove_reference<Args>::type...>;
  return std::unique_ptr<Callback>(
      new T(std::forward<Function>(f),
            std::forward_as_tuple<Args...>(std::forward<Args>(args)...)));
}

}  // namespace event

#endif  // EVENT_CALLBACK_H
