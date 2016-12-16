// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/dispatcher.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/cleanup.h"
#include "base/result.h"
#include "base/logging.h"

namespace event {

static std::size_t compute_num_cores() {
  int fd = ::open("/proc/cpuinfo", O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    int err_no = errno;
    CHECK_OK(base::Result::from_errno(err_no, "open(2)")) << ": failed to open /proc/cpuinfo";
    return 4;
  }
  auto cleanup = base::cleanup([fd] { ::close(fd); });

  std::vector<char> buf;
  std::size_t pos = 0;
  while (true) {
    buf.resize(pos + 4096);
    ssize_t n = ::read(fd, buf.data() + pos, buf.size() - pos);
    if (n < 0) {
      int err_no = errno;
      CHECK_OK(base::Result::from_errno(err_no, "read(2)")) << ": failed to read /proc/cpuinfo";
      return 4;
    }
    if (n == 0) break;
    pos += n;
  }
  buf.resize(pos);

  const char* begin = buf.data();
  const char* end = begin + buf.size();

  std::size_t cores = 0;
  std::size_t newline_run = 0;
  for (const char* ptr = begin; ptr != end; ++ptr) {
    if (*ptr == '\n') {
      ++newline_run;
      if (newline_run == 2) ++cores;
    } else {
      newline_run = 0;
    }
  }

  LOG(INFO) << "Found " << cores << " CPU core(s)";
  return cores;
}

std::size_t num_cores() {
  static std::size_t value = compute_num_cores();
  return value;
}

}  // namespace event
