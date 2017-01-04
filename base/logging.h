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

#define LOG_LEVEL_INFO ::base::level_t(1)
#define LOG_LEVEL_WARN ::base::level_t(2)
#define LOG_LEVEL_ERROR ::base::level_t(3)
#define LOG_LEVEL_DFATAL ::base::level_t(4)
#define LOG_LEVEL_FATAL ::base::level_t(5)
#define LOG_LEVEL(name) LOG_LEVEL_##name
#define VLOG_LEVEL(vlevel) ::base::level_t(-(vlevel))

#define LOG(name) ::base::Logger(__FILE__, __LINE__, 1, LOG_LEVEL(name))
#define VLOG(vlevel) ::base::Logger(__FILE__, __LINE__, 1, VLOG_LEVEL(vlevel))

#define LOG_EVERY_N(name, n) \
  ::base::Logger(__FILE__, __LINE__, (n), LOG_LEVEL(name))
#define VLOG_EVERY_N(vlevel, n) \
  ::base::Logger(__FILE__, __LINE__, (n), VLOG_LEVEL(vlevel))

#define LOG_EXCEPTION(e) \
  ::base::internal::log_exception(__FILE__, __LINE__, (e))

#define CHECK(x) ::base::internal::log_check(__FILE__, __LINE__, #x, !!(x))
#define CHECK_OK(x) ::base::internal::log_check_ok(__FILE__, __LINE__, #x, (x))

#define CHECK_EQ(x, y)                                                         \
  ::base::internal::log_check_op(__FILE__, __LINE__, ::base::internal::OpEQ(), \
                                 #x, (x), #y, (y))
#define CHECK_NE(x, y)                                                         \
  ::base::internal::log_check_op(__FILE__, __LINE__, ::base::internal::OpNE(), \
                                 #x, (x), #y, (y))
#define CHECK_LT(x, y)                                                         \
  ::base::internal::log_check_op(__FILE__, __LINE__, ::base::internal::OpLT(), \
                                 #x, (x), #y, (y))
#define CHECK_LE(x, y)                                                         \
  ::base::internal::log_check_op(__FILE__, __LINE__, ::base::internal::OpLE(), \
                                 #x, (x), #y, (y))
#define CHECK_GT(x, y)                                                         \
  ::base::internal::log_check_op(__FILE__, __LINE__, ::base::internal::OpGT(), \
                                 #x, (x), #y, (y))
#define CHECK_GE(x, y)                                                         \
  ::base::internal::log_check_op(__FILE__, __LINE__, ::base::internal::OpGE(), \
                                 #x, (x), #y, (y))

#define CHECK_NOTNULL(ptr) \
  ::base::internal::log_check_notnull(__FILE__, __LINE__, #ptr, (ptr))

#ifdef NDEBUG

#define DLOG(name) ::base::Logger(nullptr)
#define DVLOG(vlevel) ::base::Logger(nullptr)
#define DLOG_EVERY_N(name, n) ::base::Logger(nullptr)
#define DVLOG_EVERY_N(vlevel, n) ::base::Logger(nullptr)

#define DCHECK(x) ::base::Logger(nullptr)
#define DCHECK_OK(x) ::base::Logger(nullptr)

#define DCHECK_EQ(x, y) ::base::Logger(nullptr)
#define DCHECK_NE(x, y) ::base::Logger(nullptr)
#define DCHECK_LT(x, y) ::base::Logger(nullptr)
#define DCHECK_GT(x, y) ::base::Logger(nullptr)
#define DCHECK_LE(x, y) ::base::Logger(nullptr)
#define DCHECK_GE(x, y) ::base::Logger(nullptr)

#define DCHECK_NOTNULL(ptr) (ptr)

#else

#define DLOG(name) LOG(name)
#define DVLOG(vlevel) VLOG(vlevel)
#define DLOG_EVERY_N(name, n) LOG_EVERY_N(name, (n))
#define DVLOG_EVERY_N(vlevel, n) VLOG_EVERY_N((vlevel), (n))

#define DCHECK(x) CHECK(x)
#define DCHECK_OK(x) CHECK_OK(x)

#define DCHECK_EQ(x, y) CHECK_EQ((x), (y))
#define DCHECK_NE(x, y) CHECK_NE((x), (y))
#define DCHECK_LT(x, y) CHECK_LT((x), (y))
#define DCHECK_GT(x, y) CHECK_GT((x), (y))
#define DCHECK_LE(x, y) CHECK_LE((x), (y))
#define DCHECK_GE(x, y) CHECK_GE((x), (y))

#define DCHECK_NOTNULL(ptr) CHECK_NOTNULL(ptr)

#endif

namespace base {

class Result;  // forward declaration

class level_t {
 public:
  constexpr level_t() noexcept : value_(0) {}
  explicit constexpr level_t(signed char value) noexcept : value_(value) {}
  constexpr level_t(const level_t&) noexcept = default;
  constexpr level_t(level_t&&) noexcept = default;
  level_t& operator=(const level_t&) noexcept = default;
  level_t& operator=(level_t&&) noexcept = default;
  explicit constexpr operator signed char() const noexcept { return value_; }

 private:
  signed char value_;
};

inline constexpr bool operator==(level_t a, level_t b) noexcept {
  return static_cast<signed char>(a) == static_cast<signed char>(b);
}
inline constexpr bool operator!=(level_t a, level_t b) noexcept {
  return !(a == b);
}
inline constexpr bool operator<(level_t a, level_t b) noexcept {
  return static_cast<signed char>(a) < static_cast<signed char>(b);
}
inline constexpr bool operator>(level_t a, level_t b) noexcept {
  return (b < a);
}
inline constexpr bool operator<=(level_t a, level_t b) noexcept {
  return !(b < a);
}
inline constexpr bool operator>=(level_t a, level_t b) noexcept {
  return !(a < b);
}

// LogEntry represents a single log message.
struct LogEntry {
  struct timeval time;
  pid_t tid;
  const char* file;
  unsigned int line;
  level_t level;
  std::string message;

  LogEntry() noexcept;
  LogEntry(const char* file, unsigned int line, level_t level,
           std::string message) noexcept;
  LogEntry(const LogEntry& other);
  LogEntry(LogEntry&& other) noexcept;
  LogEntry& operator=(const LogEntry& other);
  LogEntry& operator=(LogEntry&& other) noexcept;

  void append_to(std::string* out) const;
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

  LogEntry entry();

 private:
  const char* file_;
  unsigned int line_;
  unsigned int n_;
  level_t level_;
  std::unique_ptr<std::ostringstream> ss_;
};

class LogTarget {
 protected:
  LogTarget() noexcept = default;

 public:
  virtual ~LogTarget() noexcept = default;
  virtual bool want(const char* file, unsigned int line, level_t level) const
      noexcept = 0;
  virtual void log(const LogEntry& entry) noexcept = 0;
};

// Force logs to be written synchronously or asynchronously.
void log_single_threaded();

// Wait for all pending logs to reach disk.
void log_flush();

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

namespace internal {

void log_exception(const char* file, unsigned int line, std::exception_ptr e);

Logger log_check(const char* file, unsigned int line, const char* expr,
                 bool cond);

Logger log_check_ok(const char* file, unsigned int line, const char* expr,
                    const Result& rslt);

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
  Logger logger(file, line, 1, LOG_LEVEL_FATAL);
  logger << "CHECK FAILED: " << expr << " != nullptr";
  std::terminate();
}

template <typename T>
std::unique_ptr<T> log_check_notnull(const char* file, unsigned int line,
                                     const char* expr, std::unique_ptr<T> ptr) {
  if (ptr) return std::move(ptr);
  Logger logger(file, line, 1, LOG_LEVEL_FATAL);
  logger << "CHECK FAILED: " << expr << " != nullptr";
  std::terminate();
}

template <typename T>
std::shared_ptr<T> log_check_notnull(const char* file, unsigned int line,
                                     const char* expr, std::shared_ptr<T> ptr) {
  if (ptr) return std::move(ptr);
  Logger logger(file, line, 1, LOG_LEVEL_FATAL);
  logger << "CHECK FAILED: " << expr << " != nullptr";
  std::terminate();
}

}  // namespace internal

}  // namespace base

#endif  // BASE_LOGGING_H
