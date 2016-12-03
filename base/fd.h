#ifndef BASE_FD_H
#define BASE_FD_H

#include <unistd.h>

#include <cerrno>
#include <memory>
#include <mutex>
#include <utility>

#include "base/result.h"

namespace base {

// FDHolder is a thread-safe class for serializing access to file descriptors.
//
// Why? Individual operations on file descriptors are necessarily thread-safe,
// but there is a race condition if |close(2)| is involved: after the original
// FD is closed, the kernel can recycle that FD number. Thus, attempts to
// operate on the original FD can be misdirected to the new FD if they aren't
// synchronized with |close(2)|.
//
class FDHolder {
 public:
  // Embeds the given file descriptor into a new FDHolder.
  static std::shared_ptr<FDHolder> make(int fd) {
    return std::make_shared<FDHolder>(fd);
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
    mu_.lock();
    return fd_;
  }

  // Low-level primitive. Unlocks the mutex.
  void unlock() const { mu_.unlock(); }

  // Acquires a lock and returns <file descriptor, lock>.
  // - If the fd was closed, returns <-1, lock>
  std::pair<int, std::unique_lock<std::mutex>> acquire() const {
    std::unique_lock<std::mutex> lock(mu_);
    return std::make_pair(fd_, std::move(lock));
  }

  // Trades file descriptors between FDHolder objects.
  void swap(FDHolder& x) {
    std::unique_lock<std::mutex> lock(mu_);
    std::swap(fd_, x.fd_);
  }

  // Relinquishes ownership of the file descriptor.
  // - This FDHolder moves to the already-closed state
  int release() {
    std::unique_lock<std::mutex> lock(mu_);
    int fd = -1;
    std::swap(fd, fd_);
    return fd;
  }

  // Acquires a lock and closes the file descriptor.
  // - If the fd was closed, fails (probably with EBADF)
  base::Result close();

 private:
  mutable std::mutex mu_;
  int fd_;
};

// FDHolder is normally used through a shared_ptr. Save some typing.
using FD = std::shared_ptr<FDHolder>;

}  // namespace base

#endif  // BASE_FD_H
