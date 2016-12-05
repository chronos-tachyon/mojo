// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/logging.h"

#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#include "base/debug.h"
#include "base/util.h"

static constexpr signed char kFatalLevel =
    base::debug ? LOG_LEVEL_DFATAL : LOG_LEVEL_FATAL;

static pid_t gettid() { return syscall(SYS_gettid); }

static std::size_t uintlen(unsigned long value) {
  if (value < 10) return 1;
  if (value < 100) return 2;
  if (value < 1000) return 3;
  if (value < 10000) return 4;
  if (value < 100000) return 5;
  double x = log10(value);
  return static_cast<std::size_t>(round(x));
}

static std::size_t intlen(long value) {
  if (value < 0)
    return 1 + uintlen(-value);
  else
    return uintlen(value);
}

namespace {

struct FileLine {
  const char* file;
  unsigned int line;

  FileLine(const char* file, unsigned int line) noexcept : file(file),
                                                           line(line) {}
};

static bool operator==(FileLine a, FileLine b) noexcept __attribute__((unused));
static bool operator==(FileLine a, FileLine b) noexcept {
  return a.line == b.line && ::strcmp(a.file, b.file) == 0;
}

static bool operator<(FileLine a, FileLine b) noexcept {
  int cmp = ::strcmp(a.file, b.file);
  return cmp < 0 || (cmp == 0 && a.line < b.line);
}

struct Info {
  std::size_t count;
  signed char level;

  Info() noexcept : count(0), level(127) {}
};

using Map = std::map<FileLine, Info>;

struct Target {
  int fd;
  signed char level;

  Target(int fd, signed char level) noexcept : fd(fd), level(level) {}
};

using Vec = std::vector<Target>;

}  // anonymous namespace

static std::mutex g_mu;
static signed char g_min = -127;
static Map* g_map = nullptr;
static Vec* g_vec = nullptr;
static base::GetTidFunc g_gtid = nullptr;
static base::GetTimeOfDayFunc g_gtod = nullptr;

static const Info* map_find(std::unique_lock<std::mutex>& lock,
                            const char* file, unsigned int line) noexcept {
  if (g_map == nullptr) return nullptr;
  auto it = g_map->find(FileLine(file, line));
  if (it == g_map->end()) return nullptr;
  return &it->second;
}

static Info* map_find_mutable(std::unique_lock<std::mutex>& lock,
                              const char* file, unsigned int line) noexcept {
  if (g_map == nullptr) g_map = new Map;
  return &(*g_map)[FileLine(file, line)];
}

static Vec& vec_get(std::unique_lock<std::mutex>& lock) {
  if (g_vec == nullptr) {
    g_vec = new Vec;
    g_vec->emplace_back(2, LOG_LEVEL_INFO);
  }
  return *g_vec;
}

static bool want(const char* file, unsigned int line, unsigned int n,
                 signed char level) {
  auto lock = base::acquire_lock(g_mu);
  if (level >= LOG_LEVEL_DFATAL) return true;
  if (level < g_min) {
    const Info* info = map_find(lock, file, line);
    if (info == nullptr || level < info->level) {
      return false;
    }
  }
  if (n > 1) {
    Info* info = map_find_mutable(lock, file, line);
    bool result = (info->count % n) == 0;
    info->count++;
    return result;
  }
  return true;
}

static std::unique_ptr<std::ostringstream> make_ss(const char* file,
                                                   unsigned int line,
                                                   unsigned int n,
                                                   signed char level) {
  std::unique_ptr<std::ostringstream> ptr;
  if (want(file, line, n, level)) ptr.reset(new std::ostringstream);
  return ptr;
}

namespace base {

Logger::Logger(std::nullptr_t)
    : file_(nullptr), line_(0), n_(0), level_(0), ss_(nullptr) {}

Logger::Logger(const char* file, unsigned int line, unsigned int every_n,
               signed char level)
    : file_(file),
      line_(line),
      n_(every_n),
      level_(level),
      ss_(make_ss(file, line, every_n, level)) {}

Logger::~Logger() noexcept(false) {
  if (ss_) {
    auto lock = acquire_lock(g_mu);

    char ch;
    if (level_ >= LOG_LEVEL_DFATAL) {
      ch = 'F';
    } else if (level_ >= LOG_LEVEL_ERROR) {
      ch = 'E';
    } else if (level_ >= LOG_LEVEL_WARN) {
      ch = 'W';
    } else if (level_ >= LOG_LEVEL_INFO) {
      ch = 'I';
    } else {
      ch = 'D';
    }

    struct timeval tv;
    ::bzero(&tv, sizeof(tv));
    if (g_gtod) {
      (*g_gtod)(&tv, nullptr);
    } else {
      ::gettimeofday(&tv, nullptr);
    }

    struct tm tm;
    ::gmtime_r(&tv.tv_sec, &tm);

    pid_t tid;
    if (g_gtid) {
      tid = (*g_gtid)();
    } else {
      tid = gettid();
    }

    std::string message = ss_->str();

    //   1     2   2  1 2  1 2  1 2  1 6      2  ?   1 ?    1 ?    2  ?       1
    //   1     3   5  6 8  9 11 1214 1521     23 -   24-    25-    27 -       28
    // "[IWEF]<mm><dd> <hh>:<mm>:<ss>.<uuuuuu>  <tid> <file>:<line>] <message>"

    std::vector<char> buf;
    const std::size_t n_tid = intlen(tid);
    const std::size_t n_file = ::strlen(file_);
    const std::size_t n_line = uintlen(line_);
    const std::size_t n_message = message.size();
    buf.resize(64 + n_tid + n_file + n_line + n_message);

    ::snprintf(buf.data(), buf.size(),
               "%c%02u%02u %02u:%02u:%02u.%06lu  %d %s:%u] %s", ch,
               tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
               tv.tv_usec, tid, file_, line_, message.c_str());
    std::size_t n = ::strlen(buf.data());
    buf.resize(n + 1);
    buf[n] = '\n';

    auto& vec = vec_get(lock);
    for (auto target : vec) {
      if (level_ >= target.level) {
        write(target.fd, buf.data(), buf.size());
      }
    }

    if (level_ >= kFatalLevel) {
      throw fatal_error();
    }
  }
}

void log_set_gettid(GetTidFunc func) {
  auto lock = acquire_lock(g_mu);
  g_gtid = func;
}

void log_set_gettimeofday(GetTimeOfDayFunc func) {
  auto lock = acquire_lock(g_mu);
  g_gtod = func;
}

void log_stderr_set_level(signed char level) { log_fd_set_level(2, level); }

void log_fd_set_level(int fd, signed char level) {
  auto lock = acquire_lock(g_mu);
  auto& vec = vec_get(lock);
  bool found = false;
  for (auto& target : vec) {
    if (target.fd == fd) {
      target.level = level;
      found = true;
      break;
    }
  }
  if (!found) {
    vec.emplace_back(fd, level);
  }
}

void log_fd_remove(int fd) {
  auto lock = acquire_lock(g_mu);
  auto& vec = vec_get(lock);
  auto it = vec.begin();
  while (it != vec.end()) {
    if (it->fd == fd) {
      vec.erase(it);
    } else {
      ++it;
    }
  }
}

void log_exception(const char* file, unsigned int line, std::exception_ptr e) {
  Logger logger(file, line, 1, LOG_LEVEL_ERROR);
  try {
    std::rethrow_exception(e);
  } catch (const null_pointer& e) {
    logger << "caught base::null_pointer\n"
           << "\t" << e.what();
  } catch (const std::system_error& e) {
    const auto& ecode = e.code();
    logger << "caught std::system_error\n"
           << "\t" << ecode.category().name() << "(" << ecode.value()
           << "): " << e.what();
  } catch (const std::exception& e) {
    logger << "caught std::exception\n"
           << "\t[" << typeid(e).name() << "]\n"
           << "\t" << e.what();
  } catch (...) {
    logger << "ERROR: caught unclassifiable exception!";
  }
}

}  // namespace base
