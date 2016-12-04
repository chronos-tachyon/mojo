// base/fd.h - Wrapper for file descriptors
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_FD_H
#define BASE_FD_H

#include <unistd.h>

#include <cerrno>
#include <memory>
#include <utility>

#include "base/result.h"
#include "base/util.h"

namespace base {

// FDHolder is normally used through a shared_ptr. Save some typing.
class FDHolder;
using FD = std::shared_ptr<FDHolder>;

// FDHolder is a thread-safe class for serializing access to file descriptors.
//
// Why? There is a race condition if |close(2)| is involved: after the
// original FD is closed, the kernel can recycle that FD number. Thus, attempts
// to operate on the original FD can be misdirected to the new FD if they
// aren't synchronized with |close(2)|.
//
class FDHolder {
 public:
  // Embeds the given file descriptor into a new FDHolder.
  static std::shared_ptr<FDHolder> make(int fd) {
    return std::make_shared<FDHolder>(fd);
  }

  // Creates a new FDHolder in the already-closed state.
  static std::shared_ptr<FDHolder> empty() {
    return std::make_shared<FDHolder>();
  }

  // Constructs an FDHolder that takes ownership of the given file descriptor.
  explicit FDHolder(int fd) noexcept : fd_(fd) {}

  // Default constructs an FDHolder in the already-closed state.
  FDHolder() noexcept : fd_(-1) {}

  // FDHolder is neither copyable nor moveable.
  FDHolder(const FDHolder&) = delete;
  FDHolder(FDHolder&&) = delete;
  FDHolder& operator=(const FDHolder&) = delete;
  FDHolder& operator=(FDHolder&&) = delete;

  // Destroying an FDHolder closes the owned file descriptor.
  ~FDHolder() noexcept { close().expect_ok(); }

  // Low-level primitive. Locks the mutex and returns the file descriptor.
  // - If the fd was closed, returns -1
  int lock() const {
    mu_.lock_read();
    return fd_;
  }

  // Low-level primitive. Unlocks the mutex.
  void unlock() const { mu_.unlock_read(); }

  // Acquires a lock and returns <file descriptor, lock>.
  // - If the fd was closed, returns <-1, lock>
  std::pair<int, RLock> acquire() const noexcept {
    RLock lock(mu_);
    return std::make_pair(fd_, std::move(lock));
  }

  // Relinquishes ownership of the file descriptor.
  // - This FDHolder moves to the already-closed state
  int release() noexcept {
    WLock lock(mu_);
    int fd = -1;
    std::swap(fd, fd_);
    return fd;
  }

  // Acquires a lock and closes the file descriptor.
  // - If the fd was closed, fails (probably with EBADF)
  base::Result close();

  explicit operator bool() const noexcept {
    RLock lock(mu_);
    return fd_ != -1;
  }

 private:
  mutable RWMutex mu_;
  int fd_;
};

struct Pipe {
  FD read;
  FD write;

  Pipe() noexcept = default;
  Pipe(FD r, FD w) noexcept : read(std::move(r)), write(std::move(w)) {}
};

struct SocketPair {
  FD left;
  FD right;

  SocketPair() noexcept = default;
  SocketPair(FD l, FD r) noexcept : left(std::move(l)), right(std::move(r)) {}
};

Result make_pipe(Pipe* out);

Result make_socketpair(SocketPair* out, int domain, int type, int protocol);

Result set_nonblock(FD fd, bool value);

Result read_exactly(FD fd, void* ptr, std::size_t len, const char* what);

Result write_exactly(FD fd, const void* ptr, std::size_t len, const char* what);

}  // namespace base

#endif  // BASE_FD_H
