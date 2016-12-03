// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/fd.h"

namespace base {

base::Result FDHolder::close() {
  std::unique_lock<std::mutex> lock(mu_);
  int fd = -1;
  std::swap(fd, fd_);
  int rc = ::close(fd);
  if (rc != 0) {
    int err_no = errno;
    return Result::from_errno(err_no, "close(2)");
  }
  return Result();
}

}  // namespace base
