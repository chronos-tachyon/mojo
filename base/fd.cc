// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/fd.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <exception>

#include "base/logging.h"
#include "base/mutex.h"

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
  *out = Pipe(wrapfd(fds[0]), wrapfd(fds[1]));
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
  *out = SocketPair(wrapfd(fds[0]), wrapfd(fds[1]));
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

Result truncate(FD fd) {
  Result r;
  auto fdpair = DCHECK_NOTNULL(fd)->acquire_fd();
  int rc = ::ftruncate(fdpair.first, 0);
  if (rc != 0) {
    int err_no = errno;
    r = Result::from_errno(err_no, "ftruncate(2)");
  }
  return r;
}

Result readdir_all(std::vector<DEntry>* out, FD fd, const char* what) {
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(fd);

  std::vector<char> buf(4096);
  long nread;
  auto fdpair = fd->acquire_fd();
  while (true) {
    nread = syscall(SYS_getdents, fdpair.first, buf.data(), buf.size());
    if (nread < 0) {
      int err_no = errno;
      return Result::from_errno(err_no, "getdents(2) from ", what);
    }
    if (nread == 0) break;

    const char* ptr = buf.data();
    const char* end = ptr + nread;
    while (ptr != end) {
      constexpr std::size_t kUL = sizeof(unsigned long);
      constexpr std::size_t kUS = sizeof(unsigned short);

      unsigned long ino;
      unsigned short reclen;
      unsigned char type;
      ::memcpy(&ino, ptr, kUL);
      ::memcpy(&reclen, ptr + 2 * kUL, kUS);
      type = *(ptr + reclen - 1);
      const char* p = ptr + 2 * kUL + kUS;
      out->emplace_back(ino, type, std::string(p));
      ptr += reclen;
    }
  }
  return Result();
}

Result read_all(std::vector<char>* out, FD fd, const char* what) {
  CHECK_NOTNULL(out);
  CHECK_NOTNULL(fd);
  auto fdpair = fd->acquire_fd();
  std::size_t pos = 0;
  while (true) {
    out->resize(pos + 4096);
    ssize_t n = ::read(fdpair.first, out->data() + pos, out->size() - pos);
    if (n < 0) {
      int err_no = errno;
      return Result::from_errno(err_no, "read(2) from ", what);
    }
    if (n == 0) break;
    pos += n;
  }
  out->resize(pos);
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
    if (n < 0) {
      int err_no = errno;
      if (err_no == EINTR) {
        VLOG(4) << "EINTR";
        continue;
      }
      r = Result::from_errno(err_no, "read(2) from ", what);
      break;
    }
    VLOG(5) << "result=" << n;
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
    VLOG(5) << "result=" << n;
    if (std::size_t(n) != len) {
      r = Result::internal("short write(2) from ", what);
      break;
    }
    break;
  }
  VLOG(4) << r.as_string();
  return r;
}

static std::vector<char> build_temppath(const char* tmpl) {
  CHECK_NOTNULL(tmpl);

  const char* tmpdir;
  if (*tmpl == '/') {
    tmpdir = "";
  } else {
    tmpdir = getenv("TMPDIR");
    if (tmpdir == nullptr) tmpdir = "/tmp";
    if (!*tmpl) tmpl = "tmp.XXXXXX";
  }

  std::size_t tmpdir_len = ::strlen(tmpdir);
  std::size_t tmpl_len = ::strlen(tmpl);

  std::vector<char> buf;
  buf.resize(tmpdir_len + tmpl_len + 2);

  char* ptr = buf.data();
  ::memcpy(ptr, tmpdir, tmpdir_len);
  buf[tmpdir_len] = '/';
  ::memcpy(ptr + tmpdir_len + 1, tmpl, tmpl_len + 1);
  return std::move(buf);
}

Result make_tempfile(std::string* path, FD* fd, const char* tmpl) {
  CHECK_NOTNULL(path);
  CHECK_NOTNULL(fd);
  CHECK_NOTNULL(tmpl);
  path->clear();
  *fd = nullptr;

  std::vector<char> buf = build_temppath(tmpl);
  int fdnum = ::mkostemp(buf.data(), O_CLOEXEC);
  if (fdnum == -1) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "mkostemp(3)");
  }
  path->assign(buf.data(), buf.size() - 1);
  *fd = base::wrapfd(fdnum);
  return base::Result();
}

Result make_tempdir(std::string* path, const char* tmpl) {
  CHECK_NOTNULL(path);
  CHECK_NOTNULL(tmpl);
  path->clear();

  std::vector<char> buf = build_temppath(tmpl);
  const char* ret = ::mkdtemp(buf.data());
  if (ret == nullptr) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "mkdtemp(3)");
  }
  path->assign(buf.data(), buf.size() - 1);
  return base::Result();
}

}  // namespace base
