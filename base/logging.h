// base/logging.h - Facility for logging error messages
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_LOGGING_H
#define BASE_LOGGING_H

#include <sys/time.h>
#include <sys/types.h>

#include <memory>
#include <ostream>
#include <sstream>
#include <string>

namespace base {

// Logger collects a single log message to be output.
class Logger {
 private:
  using BasicManip = std::ostream& (*)(std::ostream&);

 public:
  Logger(std::nullptr_t);
  Logger(const char* file, unsigned int line, unsigned int every_n,
         signed char level);
  ~Logger() noexcept(false);

  // Logger is move-only.
  Logger(const Logger&) = delete;
  Logger(Logger&&) noexcept = default;
  Logger& operator=(const Logger&) = delete;
  Logger& operator=(Logger&&) noexcept = default;

  const char* file() const noexcept { return file_; }
  unsigned int line() const noexcept { return line_; }
  unsigned int every_n() const noexcept { return n_; }
  signed char level() const noexcept { return level_; }

  explicit operator bool() const noexcept { return !!ss_; }
  std::ostringstream* stream() noexcept { return ss_.get(); }
  const std::ostringstream* stream() const noexcept { return ss_.get(); }

  std::string message() const {
    if (ss_) return ss_->str();
    return std::string();
  }

  template <typename T>
  Logger& operator<<(const T& obj) {
    if (ss_) (*ss_) << obj;
    return *this;
  }

  Logger& operator<<(BasicManip obj) {
    if (ss_) (*ss_) << obj;
    return *this;
  }

 private:
  const char* const file_;
  const unsigned int line_;
  const unsigned int n_;
  const signed char level_;
  std::unique_ptr<std::ostringstream> ss_;
};

// Exception class thrown by FATAL errors.
class fatal_error {
 public:
  constexpr fatal_error() noexcept = default;
  constexpr fatal_error(const fatal_error&) noexcept = default;
  constexpr fatal_error(fatal_error&&) noexcept = default;
  fatal_error& operator=(const fatal_error&) noexcept = default;
  fatal_error& operator=(fatal_error&&) noexcept = default;
};

// Low-level functions for routing logs {{{

void log_stderr_set_level(signed char level);
void log_fd_set_level(int fd, signed char level);
void log_fd_remove(int fd);

// }}}
// Functions for mocking in tests {{{

using GetTidFunc = pid_t (*)();
using GetTimeOfDayFunc = int (*)(struct timeval*, struct timezone*);

void log_set_gettid(GetTidFunc func);
void log_set_gettimeofday(GetTimeOfDayFunc func);

// }}}

#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_DFATAL 4
#define LOG_LEVEL_FATAL 5
#define LOG_LEVEL(name) LOG_LEVEL_##name

#define LOG(name) ::base::Logger(__FILE__, __LINE__, 1, LOG_LEVEL(name))
#define VLOG(vlevel) ::base::Logger(__FILE__, __LINE__, 1, -(vlevel))

#define LOG_EVERY_N(name, n) \
  ::base::Logger(__FILE__, __LINE__, (n), LOG_LEVEL(name))
#define VLOG_EVERY_N(vlevel, n) \
  ::base::Logger(__FILE__, __LINE__, (n), -(vlevel))

#ifdef NDEBUG
#define DLOG(name) ::base::Logger(nullptr)
#define DVLOG(vlevel) ::base::Logger(nullptr)
#define DLOG_EVERY_N(name, n) ::base::Logger(nullptr)
#define DVLOG_EVERY_N(vlevel, n) ::base::Logger(nullptr)
#else
#define DLOG(name) LOG(name)
#define DVLOG(vlevel) VLOG(vlevel)
#define DLOG_EVERY_N(name, n) LOG_EVERY_N(name, (n))
#define DVLOG_EVERY_N(vlevel, n) VLOG_EVERY_N((vlevel), (n))
#endif

}  // namespace base

#endif  // BASE_LOGGING_H
