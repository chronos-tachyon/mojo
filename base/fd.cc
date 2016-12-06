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

static void invoke(FDHolder::HookFn hook) noexcept {
  try {
    hook();
  } catch (...) {
    LOG_EXCEPTION(std::current_exception());
  }
}

FDHolder::FDHolder(int fd) noexcept : fd_(fd) {
  VLOG(2) << "FDHolder: obtained ownership of fd " << fd;
}

FDHolder::~FDHolder() noexcept {
  int fd = release_internal(true);
  if (fd != -1) {
    VLOG(3) << "FDHolder::~FDHolder performing close of fd " << fd;
    ::close(fd);
  }
}

void FDHolder::on_close(HookFn hook) {
  auto lock = acquire_write(rwmu_);
  if (fd_ == -1) {
    lock.unlock();
    invoke(std::move(hook));
  } else {
    hooks_.push_back(std::move(hook));
  }
}

int FDHolder::release_internal(bool for_close) noexcept {
  auto lock = acquire_write(rwmu_);
  int fd = -1;
  std::swap(fd, fd_);
  std::vector<HookFn> hooks = std::move(hooks_);
  lock.unlock();
  VLOG(2) << "FDHolder: relinquished ownership of fd " << fd << ", "
          << "for_close=" << std::boolalpha << for_close;
  for (auto& hook : hooks) {
    invoke(std::move(hook));
  }
  return fd;
}

int FDHolder::release_fd() noexcept { return release_internal(false); }

Result FDHolder::close() {
  int fd = release_internal(true);
  Result r;
  int rc = ::close(fd);
  if (rc != 0) {
    int err_no = errno;
    r = Result::from_errno(err_no, "close(2)");
  }
  return r;
}

Result make_pipe(Pipe* out) {
  *DCHECK_NOTNULL(out) = Pipe();
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
  *DCHECK_NOTNULL(out) = SocketPair();
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
  auto pair = DCHECK_NOTNULL(fd)->acquire_fd();
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

Result shutdown(FD fd, int how) {
  auto pair = DCHECK_NOTNULL(fd)->acquire_fd();
  int rc = ::shutdown(pair.first, how);
  if (rc != 0) {
    int err_no = errno;
    return Result::from_errno(err_no, "shutdown(2)");
  }
  return Result();
}

Result seek(off_t* out, FD fd, off_t offset, int whence) {
  auto pair = DCHECK_NOTNULL(fd)->acquire_fd();
  off_t n = ::lseek(pair.first, offset, whence);
  if (n == static_cast<off_t>(-1)) {
    int err_no = errno;
    return Result::from_errno(err_no, "lseek(2)");
  }
  if (out) *out = n;
  return Result();
}

Result read_exactly(FD fd, void* ptr, std::size_t len, const char* what) {
  Result r;
  ssize_t n;
  auto pair = DCHECK_NOTNULL(fd)->acquire_fd();
  while (true) {
    ::bzero(ptr, len);
    VLOG(4) << "base::read_exactly: "
            << "fd=" << pair.first << ", "
            << "len=" << len << ", "
            << "what=\"" << what << "\"";
    n = ::read(pair.first, ptr, len);
    int err_no = errno;
    VLOG(5) << "result=" << n;
    if (n < 0) {
      if (err_no == EINTR) {
        VLOG(4) << "EINTR";
        continue;
      }
      r = Result::from_errno(err_no, "read(2) from ", what);
      break;
    }
    if (n == 0) {
      r = Result::eof();
      break;
    }
    if (std::size_t(n) != len) {
      r = Result::internal("short read(2) from ", what);
      break;
    }
    break;
  }
  VLOG(4) << r.as_string();
  return r;
}

Result write_exactly(FD fd, const void* ptr, std::size_t len,
                     const char* what) {
  Result r;
  ssize_t n;
  auto pair = DCHECK_NOTNULL(fd)->acquire_fd();
  while (true) {
    VLOG(4) << "base::write_exactly: "
            << "fd=" << pair.first << ", "
            << "len=" << len << ", "
            << "what=\"" << what << "\"";
    n = ::write(pair.first, ptr, len);
    if (n < 0) {
      int err_no = errno;
      if (err_no == EINTR) {
        VLOG(4) << "EINTR";
        continue;
      }
      r = Result::from_errno(err_no, "write(2) from ", what);
      break;
    }
    if (std::size_t(n) != len) {
      r = Result::internal("short write(2) from ", what);
      break;
    }
    break;
  }
  VLOG(4) << r.as_string();
  return r;
}

}  // namespace base
