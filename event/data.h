// event/data.h - Defines data passed to event handlers
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef EVENT_DATA_H
#define EVENT_DATA_H

#include <type_traits>

#include "base/token.h"
#include "event/set.h"

namespace event {

// A Data struct is a collection of fields identifying which events happened,
// to what, and why.  It is used as an argument to an event::Handler.
struct Data {
  // |token| contains the token that was registered for the current handler.
  base::token_t token;

  // |fd| contains the file descriptor that provoked the event.
  //
  // - |fd == -1| iff the event did not occur on a public file descriptor.
  //
  int fd;

  // |signal_number| contains the signal which was received by the process.
  //
  // - |signal_number == 0| iff the event did not occur due to a signal.
  //
  int signal_number;

  // |signal_code| contains a POSIX-specified code detailing the source of the
  // signal, assuming that a signal was in fact received.
  //
  // If the event did not occur due to a signal, then |signal_code == 0|.
  // Depending on OS, this may be a meaningful |signal_code| value. Sorry.
  //
  // For details, see the |si_code| field of |siginfo_t| in sigaction(2).
  //
  int signal_code;

  // |int_value| is an arbitrary integer value provided as part of the event.
  //
  // - If the event was a signal sent by sigqueue(3), pthread_sigqueue(3), or
  //   the like, then this will be set to |si_value.sival_int| field of
  //   |siginfo_t|, which is the value passed by the userspace process that
  //   asked for the signal to be sent.
  //   SEE ALSO: |pid|, |uid|
  //
  // - If the event was a timer expiration, then this will be set to the
  //   number of timer events that were queued.
  //
  //   In other words, for timers |int_value >= 1|, and is greater than 1 only
  //   if the event::Manager is lagging.
  //
  // - If the event is a generic event, then this will be the value provided.
  //
  int int_value;

  // |wait_status| is the status of the child process, as with the wait(2)
  // family of system calls. This field is only populated if the event was
  // caused by a SIGCHLD signal sent by the kernel.
  //
  int wait_status;

  // |pid| is a process ID associated with the event.
  //
  // - If the event was a signal sent by kill(2), sigqueue(3), or friends, then
  //   this is the process ID of the sender.
  //   SEE ALSO: |uid|, |int_value|
  //
  // - If the event was a SIGCHLD signal sent by the kernel, then this is the
  //   process ID of the child that exited.
  //   SEE ALSO: |uid|, |wait_status|
  //
  int32_t pid;

  // |uid| is a user ID associated with the event.
  //
  // - If the event was a signal sent by kill(2), sigqueue(3), or friends, then
  //   this is the real user ID of the sender.
  //   SEE ALSO: |pid|, |int_value|
  //
  // - If the event was a SIGCHLD signal sent by the kernel, then this is the
  //   real user ID of the child that exited.
  //   SEE ALSO: |pid|, |wait_status|
  //
  int32_t uid;

  // |events| contains the boolean flags for the events that were received.
  //
  // - Events for FDs: readable, writable, priority, hangup, error
  // - Events for signals: signal
  // - Events for timers: timer
  // - Events for generic events: event
  //
  Set events;

  // Data is default constructible, copyable, and moveable.
  // There is intentionally no constructor for aggregate initialization.
  Data() noexcept : token(),
                    fd(-1),
                    signal_number(0),
                    signal_code(0),
                    int_value(0),
                    wait_status(0),
                    pid(-1),
                    uid(-1) {}
  Data(const Data&) noexcept = default;
  Data(Data&&) noexcept = default;
  Data& operator=(const Data&) noexcept = default;
  Data& operator=(Data&&) noexcept = default;
};

#if 0
// The GNU ISO C++ Library doesn't implement std::is_trivially_copyable!
static_assert(std::is_trivially_copyable<Data>::value,
              "event::Data must be trivially copyable");
#endif

static_assert(std::is_standard_layout<Data>::value,
              "event::Data must be standard layout");

}  // namespace event

#endif  // EVENT_DATA_H
