// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/fd.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <exception>

#include "base/logging.h"
#include "base/util.h"

namespace base {

FDHolder::FDHolder(int fd) noexcept : fd_(fd) {
  VLOG(0) << "FDHolder: obtained ownership of fd " << fd;
}

FDHolder::~FDHolder() noexcept {
  auto lock = acquire_write(rwmu_);
  if (fd_ != -1) {
    VLOG(0) << "FDHolder::~FDHolder for fd " << fd_;
    ::close(fd_);
  }
}

int FDHolder::release_fd() noexcept {
  auto lock = acquire_write(rwmu_);
  int fd = -1;
  std::swap(fd, fd_);
  VLOG(0) << "FDHolder: relinquished ownership of fd " << fd;
  return fd;
}

Result FDHolder::close() {
  auto lock = acquire_write(rwmu_);
  int fd = -1;
  std::swap(fd, fd_);
  Result r;
  int rc = ::close(fd);
  if (rc != 0) {
    int err_no = errno;
    r = Result::from_errno(err_no, "close(2)");
  }
  VLOG(0) << "FDHolder: closed fd " << fd << ": " << r;
  return r;
}

Result make_pipe(Pipe* out) {
  *DASSERT_NOTNULL(out) = Pipe();
  int fds[2] = {-1, -1};
  int rc = ::pipe2(fds, O_NONBLOCK | O_CLOEXEC);
  if (rc != 0) {
    int err_no = errno;
    return Result::from_errno(err_no, "pipe2(2)");
  }
  *out = Pipe(FDHolder::make(fds[0]), FDHolder::make(fds[1]));
  return Result();
}

Result make_socketpair(SocketPair* out, int domain, int type, int protocol) {
  *DASSERT_NOTNULL(out) = SocketPair();
  type |= SOCK_NONBLOCK;
  type |= SOCK_CLOEXEC;
  int fds[2] = {-1, -1};
  int rc = ::socketpair(domain, type, protocol, fds);
  if (rc != 0) {
    int err_no = errno;
    return Result::from_errno(err_no, "socketpair(2)");
  }
  *out = SocketPair(FDHolder::make(fds[0]), FDHolder::make(fds[1]));
  return Result();
}

Result set_blocking(FD fd, bool value) {
  auto pair = DASSERT_NOTNULL(fd)->acquire_fd();
  int flags = ::fcntl(pair.first, F_GETFL);
  if (flags == -1) {
    int err_no = errno;
    return Result::from_errno(err_no, "fcntl(2)");
  }
  if (value)
    flags &= ~O_NONBLOCK;
  else
    flags |= O_NONBLOCK;
  int n = ::fcntl(pair.first, F_SETFL, flags);
  if (n == -1) {
    int err_no = errno;
    return Result::from_errno(err_no, "fcntl(2)");
  }
  return Result();
}

Result read_exactly(FD fd, void* ptr, std::size_t len, const char* what) {
  auto pair = DASSERT_NOTNULL(fd)->acquire_fd();
  int n;
redo:
  ::bzero(ptr, len);
  n = ::read(pair.first, ptr, len);
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    return Result::from_errno(err_no, "read(2) from ", what);
  }
  if (n == 0) return Result::eof();
  if (std::size_t(n) != len) {
    return Result::internal("short read(2) from ", what);
  }
  return Result();
}

Result write_exactly(FD fd, const void* ptr, std::size_t len,
                     const char* what) {
  auto pair = DASSERT_NOTNULL(fd)->acquire_fd();
  int n;
redo:
  n = ::write(pair.first, ptr, len);
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    return Result::from_errno(err_no, "write(2) from ", what);
  }
  if (std::size_t(n) != len) {
    return Result::internal("short write(2) from ", what);
  }
  return Result();
}

}  // namespace base
