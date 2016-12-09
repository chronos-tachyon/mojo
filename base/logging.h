// base/logging.h - Facility for logging error messages
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_LOGGING_H
#define BASE_LOGGING_H

#include <sys/time.h>
#include <sys/types.h>

#include <exception>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>

namespace base {

using level_t = signed char;

#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_DFATAL 4
#define LOG_LEVEL_FATAL 5
#define LOG_LEVEL(name) LOG_LEVEL_##name
#define VLOG_LEVEL(vlevel) (-(vlevel))

// Exception class thrown by LOG(FATAL) errors.
class fatal_error {
 public:
  fatal_error() noexcept = default;
  fatal_error(const fatal_error&) noexcept = default;
  fatal_error(fatal_error&&) noexcept = default;
  fatal_error& operator=(const fatal_error&) noexcept = default;
  fatal_error& operator=(fatal_error&&) noexcept = default;
};

// Exception class thrown by CHECK_NOTNULL errors.
class null_pointer {
 public:
  null_pointer() noexcept : what_("") {}
  null_pointer(const char* what) noexcept : what_(what ? what : "") {}
  null_pointer(const null_pointer&) noexcept = default;
  null_pointer(null_pointer&&) noexcept = default;
  null_pointer& operator=(const null_pointer&) noexcept = default;
  null_pointer& operator=(null_pointer&&) noexcept = default;
  const char* what() const noexcept { return what_; }

 private:
  const char* what_;
};

// LogEntry represents a single log message.
struct LogEntry {
  struct timeval time;
  pid_t tid;
  const char* file;
  unsigned int line;
  level_t level;
  std::string message;

  LogEntry(const char* file, unsigned int line, level_t level, std::string message) noexcept;
  void append_to(std::string& out) const;
  std::string as_string() const;
};

// Logger collects a single log message to be output.
class Logger {
 private:
  using BasicManip = std::ostream& (*)(std::ostream&);

 public:
  Logger(std::nullptr_t);
  Logger(const char* file, unsigned int line, unsigned int every_n,
         level_t level);
  ~Logger() noexcept(false);

  // Logger is move-only.
  Logger(const Logger&) = delete;
  Logger(Logger&&) noexcept = default;
  Logger& operator=(const Logger&) = delete;
  Logger& operator=(Logger&&) noexcept = default;

  const char* file() const noexcept { return file_; }
  unsigned int line() const noexcept { return line_; }
  unsigned int every_n() const noexcept { return n_; }
  level_t level() const noexcept { return level_; }

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
  const level_t level_;
  std::unique_ptr<std::ostringstream> ss_;
};

class LogTarget {
 protected:
  LogTarget() noexcept = default;

 public:
  virtual ~LogTarget() noexcept = default;
  virtual bool want(const char* file, unsigned int line, level_t level) const noexcept = 0;
  virtual void log(const LogEntry& entry) noexcept = 0;
};

// Low-level functions for routing logs {{{

void log_stderr_set_level(level_t level);
void log_target_add(LogTarget* target);
void log_target_remove(LogTarget* target);

// }}}
// Functions for mocking in tests {{{

using GetTidFunc = pid_t (*)();
using GetTimeOfDayFunc = int (*)(struct timeval*, struct timezone*);

void log_set_gettid(GetTidFunc func);
void log_set_gettimeofday(GetTimeOfDayFunc func);

// }}}

void log_exception(const char* file, unsigned int line, std::exception_ptr e);

Logger log_check(const char* file, unsigned int line, const char* expr,
                 bool cond);

template <typename T, typename U, typename Predicate>
Logger log_check_op(const char* file, unsigned int line, Predicate pred,
                    const char* lhsexpr, const T& lhs, const char* rhsexpr,
                    const U& rhs) {
  if (pred(lhs, rhs)) return Logger(nullptr);
  const char* op = pred.name();
  Logger logger(file, line, 1, LOG_LEVEL_DFATAL);
  logger << "CHECK FAILED: " << lhsexpr << " " << op << " " << rhsexpr << " "
         << "[" << lhs << " " << op << " " << rhs << "]";
  return logger;
}

struct OpEQ {
  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    return (lhs == rhs);
  }
  const char* name() const { return "=="; }
};
struct OpNE {
  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    return !(lhs == rhs);
  }
  const char* name() const { return "!="; }
};
struct OpLT {
  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    return (lhs < rhs);
  }
  const char* name() const { return "<"; }
};
struct OpGT {
  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    return (rhs < lhs);
  }
  const char* name() const { return ">"; }
};
struct OpLE {
  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    return !(rhs < lhs);
  }
  const char* name() const { return "<="; }
};
struct OpGE {
  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    return !(lhs < rhs);
  }
  const char* name() const { return ">="; }
};

template <typename T>
T* log_check_notnull(const char* file, unsigned int line, const char* expr,
                     T* ptr) {
  if (ptr) return ptr;
  try {
    Logger logger(file, line, 1, LOG_LEVEL_FATAL);
    logger << "CHECK FAILED: " << expr << " != nullptr";
  } catch (const fatal_error& e) {
    // pass
  }
  throw null_pointer(expr);
}

template <typename T>
std::unique_ptr<T> log_check_notnull(const char* file, unsigned int line,
                                     const char* expr, std::unique_ptr<T> ptr) {
  if (ptr) return std::move(ptr);
  try {
    Logger logger(file, line, 1, LOG_LEVEL_FATAL);
    logger << "CHECK FAILED: " << expr << " != nullptr";
  } catch (const fatal_error& e) {
    // pass
  }
  throw null_pointer(expr);
}

template <typename T>
std::shared_ptr<T> log_check_notnull(const char* file, unsigned int line,
                                     const char* expr, std::shared_ptr<T> ptr) {
  if (ptr) return std::move(ptr);
  try {
    Logger logger(file, line, 1, LOG_LEVEL_FATAL);
    logger << "CHECK FAILED: " << expr << " != nullptr";
  } catch (const fatal_error& e) {
    // pass
  }
  throw null_pointer(expr);
}

Logger force_eval(bool);

#define LOG(name) ::base::Logger(__FILE__, __LINE__, 1, LOG_LEVEL(name))
#define VLOG(vlevel) ::base::Logger(__FILE__, __LINE__, 1, VLOG_LEVEL(vlevel))

#define LOG_EVERY_N(name, n) \
  ::base::Logger(__FILE__, __LINE__, (n), LOG_LEVEL(name))
#define VLOG_EVERY_N(vlevel, n) \
  ::base::Logger(__FILE__, __LINE__, (n), VLOG_LEVEL(vlevel))

#define LOG_EXCEPTION(e) ::base::log_exception(__FILE__, __LINE__, (e))

#define CHECK(x) ::base::log_check(__FILE__, __LINE__, #x, !!(x))

#define CHECK_EQ(x, y) \
  ::base::log_check_op(__FILE__, __LINE__, ::base::OpEQ(), #x, (x), #y, (y))
#define CHECK_NE(x, y) \
  ::base::log_check_op(__FILE__, __LINE__, ::base::OpNE(), #x, (x), #y, (y))
#define CHECK_LT(x, y) \
  ::base::log_check_op(__FILE__, __LINE__, ::base::OpLT(), #x, (x), #y, (y))
#define CHECK_LE(x, y) \
  ::base::log_check_op(__FILE__, __LINE__, ::base::OpLE(), #x, (x), #y, (y))
#define CHECK_GT(x, y) \
  ::base::log_check_op(__FILE__, __LINE__, ::base::OpGT(), #x, (x), #y, (y))
#define CHECK_GE(x, y) \
  ::base::log_check_op(__FILE__, __LINE__, ::base::OpGE(), #x, (x), #y, (y))

#define CHECK_NOTNULL(ptr) \
  ::base::log_check_notnull(__FILE__, __LINE__, #ptr, (ptr))

#ifdef NDEBUG

#define DLOG(name) ::base::Logger(nullptr)
#define DVLOG(vlevel) ::base::Logger(nullptr)
#define DLOG_EVERY_N(name, n) ::base::Logger(nullptr)
#define DVLOG_EVERY_N(vlevel, n) ::base::Logger(nullptr)

#define DCHECK(x) ::base::force_eval(x)

#define DCHECK_EQ(x, y) ::base::force_eval((x) == (y))
#define DCHECK_NE(x, y) ::base::force_eval(!((x) == (y)))
#define DCHECK_LT(x, y) ::base::force_eval((x) < (y))
#define DCHECK_GT(x, y) ::base::force_eval((y) < (x))
#define DCHECK_LE(x, y) ::base::force_eval(!((y) < (x)))
#define DCHECK_GE(x, y) ::base::force_eval(!((x) < (y)))

#define DCHECK_NOTNULL(ptr) (ptr)

#else

#define DLOG(name) LOG(name)
#define DVLOG(vlevel) VLOG(vlevel)
#define DLOG_EVERY_N(name, n) LOG_EVERY_N(name, (n))
#define DVLOG_EVERY_N(vlevel, n) VLOG_EVERY_N((vlevel), (n))

#define DCHECK(x) CHECK(x)

#define DCHECK_EQ(x, y) CHECK_EQ((x), (y))
#define DCHECK_NE(x, y) CHECK_NE((x), (y))
#define DCHECK_LT(x, y) CHECK_LT((x), (y))
#define DCHECK_GT(x, y) CHECK_GT((x), (y))
#define DCHECK_LE(x, y) CHECK_LE((x), (y))
#define DCHECK_GE(x, y) CHECK_GE((x), (y))

#define DCHECK_NOTNULL(ptr) CHECK_NOTNULL(ptr)

#endif

}  // namespace base

#endif  // BASE_LOGGING_H
