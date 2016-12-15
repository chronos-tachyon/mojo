// event/poller.h - Polling for events
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef EVENT_POLLER_H
#define EVENT_POLLER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "base/fd.h"
#include "base/result.h"
#include "base/token.h"
#include "event/set.h"

namespace event {

// PollerType is used to identify which I/O polling strategy to use.
enum class PollerType : uint8_t {
  // Let the system pick a Poller implementation.
  unspecified = 0,

  // Use BSD select(2).
  select_poller = 1,

  // Use POSIX poll(2).
  poll_poller = 2,

  // Use Linux epoll(7).
  epoll_poller = 3,
};

// A Poller is a wrapper around a non-blocking I/O notification mechanism.
// This is a little low-level for most people's tastes; event::Manager is a
// wrapper around this that provides much more extensive multiplexing.
//
// THREAD SAFETY: This class is thread-safe.
//
class Poller {
 protected:
  Poller() noexcept = default;

 public:
  using Event = std::pair<base::token_t, Set>;
  using EventVec = std::vector<Event>;

  virtual ~Poller() noexcept = default;

  // Pollers are neither copyable nor moveable.
  Poller(const Poller&) = delete;
  Poller(Poller&&) = delete;
  Poller& operator=(const Poller&) = delete;
  Poller& operator=(Poller&&) = delete;

  // Returns the type of this Poller.
  virtual PollerType type() const noexcept = 0;

  // Registers a file descriptor and a set of events.
  // Analogous to epoll_ctl(EPOLL_CTL_ADD).
  virtual base::Result add(base::FD fd, base::token_t t, Set set) = 0;

  // Modifies the set of events associated with a file descriptor.
  // Analogous to epoll_ctl(EPOLL_CTL_MOD).
  virtual base::Result modify(base::FD fd, base::token_t t, Set set) = 0;

  // Cancels the registration of a file descriptor.
  // Analogous to epoll_ctl(EPOLL_CTL_DEL).
  virtual base::Result remove(base::FD fd) = 0;

  // Waits for events to arrive on file descriptors.
  // - If |timeout_ms < 0|, blocks indefinitely until an event comes in.
  // - If |timeout_ms > 0|, blocks for the specified number of milliseconds.
  // - If |timeout_ms == 0|, does not block.
  //
  // Upon return, the pending events (if any) have been appended to |out|, in
  // the form of <token, witnessed events> pairs.
  //
  // NOTE: |out| is not cleared by this function before appending events.
  virtual base::Result wait(EventVec* out, int timeout_ms) const = 0;
};

// A PollerOptions holds user-available choices in the selection and
// configuration of Poller instances.
class PollerOptions {
 public:
  // PollerOptions is default constructible, copyable, and moveable.
  // There is intentionally no constructor for aggregate initialization.
  PollerOptions() noexcept : type_(PollerType::unspecified) {}
  PollerOptions(const PollerOptions&) = default;
  PollerOptions(PollerOptions&&) noexcept = default;
  PollerOptions& operator=(const PollerOptions&) = default;
  PollerOptions& operator=(PollerOptions&&) noexcept = default;

  // Resets all fields to their default values.
  void reset() noexcept { *this = PollerOptions(); }

  // The |type()| value is used by new_poller to override which
  // Poller implementation will be constructed. If |type()| is
  // |PollerType::unspecified|, then a suitable default will be selected.
  PollerType type() const noexcept { return type_; }
  void reset_type() noexcept { type_ = PollerType::unspecified; }
  void set_type(PollerType type) noexcept { type_ = type; }

 private:
  PollerType type_;
};

// Constructs a new Poller instance.
base::Result new_poller(std::shared_ptr<Poller>* out,
                        const PollerOptions& opts);

}  // namespace event

#endif  // EVENT_POLLER_H
