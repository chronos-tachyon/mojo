// base/fd.h - Wrapper for file descriptors
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_FD_H
#define BASE_FD_H

#include <sys/types.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "base/mutex.h"
#include "base/result.h"

namespace base {

// FDHolder is a thread-safe class for serializing access to file descriptors.
//
// Why? There is a race condition if |close(2)| is involved: after the
// original FD is closed, the kernel can recycle that FD number. Thus, attempts
// to operate on the original FD can be misdirected to the new FD if they
// aren't synchronized with |close(2)|.
//
class FDHolder {
 public:
  using HookFn = std::function<void()>;

  // Constructs an FDHolder that takes ownership of the given file descriptor.
  explicit FDHolder(int fd) noexcept;

  // FDHolder is neither copyable nor moveable.
  FDHolder(const FDHolder&) = delete;
  FDHolder(FDHolder&&) = delete;
  FDHolder& operator=(const FDHolder&) = delete;
  FDHolder& operator=(FDHolder&&) = delete;

  // Destroying an FDHolder closes the owned file descriptor.
  ~FDHolder() noexcept;

  // Asks to be notified when this FDHolder is closed or released.
  void on_close(HookFn hook);

  // Acquires a lock and returns <file descriptor, lock>.
  // - If the fd was closed, returns <-1, lock>
  std::pair<int, RLock> acquire_fd() const noexcept {
    auto lock = acquire_read(rwmu_);
    return std::make_pair(fd_, std::move(lock));
  }

  // Relinquishes ownership of the file descriptor.
  // - This FDHolder moves to the already-closed state
  // - Calls any on_close hooks(!)
  int release_fd() noexcept;

  // Acquires a lock and closes the file descriptor.
  // - If the fd was closed, fails (probably with EBADF)
  // - Calls any on_close hooks
  base::Result close();

  explicit operator bool() const noexcept {
    auto lock = acquire_read(rwmu_);
    return fd_ != -1;
  }

 private:
  int release_internal(bool for_close) noexcept;

  mutable RWMutex rwmu_;
  int fd_;
  std::vector<HookFn> hooks_;
};

// FDHolder is normally used through a shared_ptr. Save some typing.
using FD = std::shared_ptr<FDHolder>;

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

// Embeds the given file descriptor into a new FDHolder.
inline FD wrapfd(int fdnum) {
  return std::make_shared<FDHolder>(fdnum);
}

Result make_pipe(Pipe* out);
Result make_socketpair(SocketPair* out, int domain, int type, int protocol);
Result set_blocking(FD fd, bool value);
Result shutdown(FD fd, int how);
Result seek(off_t* out, FD fd, off_t offset, int whence);

Result read_exactly(FD fd, void* ptr, std::size_t len, const char* what);
Result write_exactly(FD fd, const void* ptr, std::size_t len, const char* what);

Result make_tempfile(std::string* path, FD* fd, const char* tmpl);

}  // namespace base

#endif  // BASE_FD_H
