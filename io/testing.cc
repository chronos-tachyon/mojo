// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/testing.h"

#include <sys/syscall.h>
#include <unistd.h>

namespace io {

pid_t gettid() noexcept { return syscall(SYS_gettid); }

}  // namespace io
