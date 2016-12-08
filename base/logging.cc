// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/logging.h"

#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "base/concat.h"
#include "base/debug.h"
#include "base/util.h"

namespace base {

static pid_t gettid() { return syscall(SYS_gettid); }

namespace {
struct Key {
  const char* file;
  unsigned int line;

  Key(const char* file, unsigned int line) noexcept : file(file), line(line) {}
};

static bool operator==(Key a, Key b) noexcept __attribute__((unused));
static bool operator==(Key a, Key b) noexcept {
  return a.line == b.line && ::strcmp(a.file, b.file) == 0;
}

static bool operator<(Key a, Key b) noexcept {
  int cmp = ::strcmp(a.file, b.file);
  return cmp < 0 || (cmp == 0 && a.line < b.line);
}
}  // anonymous namespace

using Map = std::map<Key, std::size_t>;
using Vec = std::vector<LogTarget*>;
using Queue = std::queue<LogEntry>;

static std::mutex g_mu;
static std::mutex g_queue_mu;
static std::condition_variable g_queue_put_cv;
static std::condition_variable g_queue_empty_cv;
static std::once_flag g_once;
static level_t g_stderr = LOG_LEVEL_INFO;
static GetTidFunc g_gtid = nullptr;
static GetTimeOfDayFunc g_gtod = nullptr;

static base::LogTarget* make_stderr();

static Map& map_get(std::unique_lock<std::mutex>& lock) {
  static Map& m = *new Map;
  return m;
}

static Vec& vec_get(std::unique_lock<std::mutex>& lock) {
  static Vec& v = *new Vec;
  if (v.empty()) {
    v.push_back(make_stderr());
  }
  return v;
}

static Queue& queue_get(std::unique_lock<std::mutex>& lock) {
  static Queue& q = *new Queue;
  return q;
}

static void thread_body() {
  auto lock0 = base::acquire_lock(g_queue_mu);
  auto& q = queue_get(lock0);
  while (true) {
    while (q.empty()) g_queue_put_cv.wait(lock0);
    auto entry = std::move(q.front());
    q.pop();
    auto lock1 = base::acquire_lock(g_mu);
    auto& v = vec_get(lock1);
    for (const auto& target : v) {
      if (target->want(entry.file, entry.line, entry.level))
        target->log(entry);
    }
    if (q.empty()) g_queue_empty_cv.notify_all();
  }
}

static bool want(const char* file, unsigned int line, unsigned int n,
                 level_t level) {
  if (level >= LOG_LEVEL_DFATAL) return true;
  auto lock = base::acquire_lock(g_mu);
  if (n > 1) {
    auto& m = map_get(lock);
    Key key(file, line);
    auto pair = m.insert(std::make_pair(key, 0));
    auto& count = pair.first->second;
    bool x = (count == 0);
    count = (count + 1) % n;
    if (!x) return false;
  }
  auto& v = vec_get(lock);
  for (const auto& target : v) {
    if (target->want(file, line, level)) return true;
  }
  return false;
}

static void log(LogEntry entry) {
  std::call_once(g_once, [] { std::thread(thread_body).detach(); });
  auto lock = base::acquire_lock(g_queue_mu);
  auto& q = queue_get(lock);
  q.push(std::move(entry));
  g_queue_put_cv.notify_one();
}

namespace {
class LogSTDERR : public LogTarget {
 public:
  LogSTDERR() noexcept = default;
  bool want(const char* file, unsigned int line, level_t level) const noexcept override {
    // g_mu held by base::want()
    return level >= g_stderr;
  }
  void log(const LogEntry& entry) noexcept override {
    // g_mu held by base::thread_body()
    auto str = entry.as_string();
    ::write(2, str.data(), str.size());
  }
};
}  // anonymous namespace

static base::LogTarget* make_stderr() {
  static base::LogTarget* const ptr = new LogSTDERR;
  return ptr;
}

LogEntry::LogEntry(const char* file, unsigned int line, level_t level,
                   std::string message) noexcept : file(file),
                                                   line(line),
                                                   level(level),
                                                   message(std::move(message)) {
  auto lock = acquire_lock(g_mu);
  ::bzero(&time, sizeof(time));
  if (g_gtod) {
    (*g_gtod)(&time, nullptr);
  } else {
    ::gettimeofday(&time, nullptr);
  }
  if (g_gtid) {
    tid = (*g_gtid)();
  } else {
    tid = gettid();
  }
}

void LogEntry::append_to(std::string& out) const {
  char ch;
  if (level >= LOG_LEVEL_DFATAL) {
    ch = 'F';
  } else if (level >= LOG_LEVEL_ERROR) {
    ch = 'E';
  } else if (level >= LOG_LEVEL_WARN) {
    ch = 'W';
  } else if (level >= LOG_LEVEL_INFO) {
    ch = 'I';
  } else {
    ch = 'D';
  }

  struct tm tm;
  ::gmtime_r(&time.tv_sec, &tm);

  // "[IWEF]<mm><dd> <hh>:<mm>:<ss>.<uuuuuu>  <tid> <file>:<line>] <message>"

  std::array<char, 24> buf;
  ::snprintf(buf.data(), buf.size(), "%c%02u%02u %02u:%02u:%02u.%06lu  ", ch,
             tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
             time.tv_usec);
  concat_to(out, std::string(buf.data()), tid, ' ', file, ':', line, "] ", message, '\n');
}

std::string LogEntry::as_string() const {
  std::string out;
  append_to(out);
  return out;
}

static std::unique_ptr<std::ostringstream> make_ss(const char* file,
                                                   unsigned int line,
                                                   unsigned int n,
                                                   level_t level) {
  std::unique_ptr<std::ostringstream> ptr;
  if (want(file, line, n, level)) ptr.reset(new std::ostringstream);
  return ptr;
}

Logger::Logger(std::nullptr_t)
    : file_(nullptr), line_(0), n_(0), level_(0), ss_(nullptr) {}

Logger::Logger(const char* file, unsigned int line, unsigned int every_n,
               level_t level)
    : file_(file),
      line_(line),
      n_(every_n),
      level_(level),
      ss_(make_ss(file, line, every_n, level)) {}

Logger::~Logger() noexcept(false) {
  if (ss_) {
    log(LogEntry(file_, line_, level_, ss_->str()));
    if (level_ >= LOG_LEVEL_DFATAL) {
      if (level_ >= LOG_LEVEL_FATAL || debug()) {
        throw fatal_error();
      }
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

void log_stderr_set_level(level_t level) {
  auto lock = acquire_lock(g_mu);
  g_stderr = level;
}

void log_target_add(LogTarget* target) {
  auto lock = acquire_lock(g_mu);
  auto& v = vec_get(lock);
  v.push_back(target);
}

void log_target_remove(LogTarget* target) {
  auto lock0 = base::acquire_lock(g_queue_mu);
  auto& q = queue_get(lock0);
  while (!q.empty()) g_queue_empty_cv.wait(lock0);
  auto lock1 = acquire_lock(g_mu);
  auto& v = vec_get(lock1);
  auto begin = v.begin(), it = v.end();
  while (it != begin) {
    --it;
    if (*it == target) {
      v.erase(it);
    }
  }
}

void log_exception(const char* file, unsigned int line, std::exception_ptr e) {
  Logger logger(file, line, 1, LOG_LEVEL_ERROR);
  try {
    std::rethrow_exception(e);
  } catch (const fatal_error& e) {
    logger << "caught base::fatal_error";
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
    logger << "caught unclassifiable exception!";
  }
}

Logger log_check(const char* file, unsigned int line, const char* expr,
                 bool cond) {
  if (cond) return Logger(nullptr);
  Logger logger(file, line, 1, LOG_LEVEL_DFATAL);
  logger << "CHECK FAILED: " << expr;
  return logger;
}

Logger force_eval(bool) { return Logger(nullptr); }

}  // namespace base
