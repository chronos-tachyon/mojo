// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/manager.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/cleanup.h"
#include "base/util.h"

using EventVec = std::vector<std::pair<int, event::Set>>;
using CallbackVec = std::vector<std::unique_ptr<event::Callback>>;

namespace {

template <typename T>
static void vec_erase_all(std::vector<T>& vec, const T& item) noexcept {
  auto begin = vec.begin(), it = vec.end();
  while (it != begin) {
    --it;
    if (*it == item) {
      auto tmp = it - 1;
      vec.erase(it);
      it = tmp + 1;
    }
  }
}

static base::Result read_exactly(int fd, void* ptr, std::size_t len,
                                 const char* what) noexcept {
  int n;
redo:
  ::bzero(ptr, len);
  n = ::read(fd, ptr, len);
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    return base::Result::from_errno(err_no, "read(2) from ", what);
  }
  if (std::size_t(n) != len) {
    return base::Result::internal("short read(2) from ", what);
  }
  return base::Result();
}

static base::Result write_exactly(int fd, const void* ptr, std::size_t len,
                                  const char* what) noexcept {
  int n;
redo:
  n = ::write(fd, ptr, len);
  if (n < 0) {
    int err_no = errno;
    if (err_no == EINTR) goto redo;
    return base::Result::from_errno(err_no, "write(2) from ", what);
  }
  if (std::size_t(n) != len) {
    return base::Result::internal("short write(2) from ", what);
  }
  return base::Result();
}

struct Pipe {
  int read_fd;
  int write_fd;

  Pipe(int rfd, int wfd) noexcept : read_fd(rfd), write_fd(wfd) {}
  Pipe() noexcept : Pipe(-1, -1) {}
};

static base::Result make_pipe(Pipe* out) noexcept {
  *out = Pipe();
  int pipefds[2];
  int rc = ::pipe2(pipefds, O_CLOEXEC);
  if (rc != 0) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "pipe2(2)");
  }
  out->read_fd = pipefds[0];
  out->write_fd = pipefds[1];
  return base::Result();
}

// Signal handler implementation details {{{

static constexpr std::size_t NUM_SIGNALS =
#ifdef _NSIG
    _NSIG
#else
    64  // reasonable guess
#endif
    ;

static void assert_valid_signo(int signo) {
  if (signo < 0 || std::size_t(signo) >= NUM_SIGNALS)
    throw std::out_of_range("invalid signal number");
}

// Guards: g_sig_pipe_rfd, g_sig_pipe_wfd, g_sig_tee
static std::mutex g_sig_mu;

// FD for the write end of the signal handler pipe.
// Only the signal handler itself should write to this pipe.
// THREAD SAFETY NOTE: this value MUST remain constant after initialization
static int g_sig_pipe_wfd = -1;

// FD for the read end of the signal handler pipe.
// Only the background thread should read from this pipe.
// THREAD SAFETY NOTE: this value MUST remain constant after initialization
static int g_sig_pipe_rfd = -1;

// FDs interested in receiving signals, arranged by signal number.
// When the background thread reads an event, it will tee into these FDs.
static std::unordered_map<int, std::vector<int>>* g_sig_tee = nullptr;

// This is the actual signal handler.
// Happily, write(2) is safe to call from within a signal handler.
// Sadly, we can't do much error checking.
static void sigaction_handler(int signo, siginfo_t* si, void* context) {
  write(g_sig_pipe_wfd, si, sizeof(*si));
}

// This parses out the guts of a siginfo_t and makes event::Data sausage.
static void populate_data_from_siginfo(event::Data* out, const siginfo_t& si) {
  out->events = event::Set::signal_bit();
  out->signal_number = si.si_signo;
  out->signal_code = si.si_code;
  switch (si.si_code) {
    case SI_USER:
    case SI_TKILL:
    case SI_QUEUE:
      out->pid = si.si_pid;
      out->uid = si.si_uid;
      if (si.si_code == SI_QUEUE) {
        out->int_value = si.si_value.sival_int;
      }
      break;

    default:
      switch (si.si_signo) {
        case SIGCHLD:
          out->pid = si.si_pid;
          out->uid = si.si_uid;
          out->wait_status = si.si_status;
          break;

        case SIGPOLL:
          out->fd = si.si_fd;
          break;
      }
  }
}

// This thread services the read end of the signal handler pipe.
static void signal_thread_body() {
  base::Lock lock(g_sig_mu, std::defer_lock);
  std::vector<int> vec;
  siginfo_t si;
  base::Result result;
  while (true) {
    result = read_exactly(g_sig_pipe_rfd, &si, sizeof(si), "signal pipe");
    result.expect_ok();
    if (!result.ok()) continue;

    lock.lock();
    auto it = g_sig_tee->find(si.si_signo);
    if (it == g_sig_tee->end())
      vec.clear();
    else
      vec = it->second;
    lock.unlock();

    event::Data data;
    populate_data_from_siginfo(&data, si);
    for (int tee_fd : vec) {
      result = write_exactly(tee_fd, &data, sizeof(data), "pipe");
      result.expect_ok();
    }
  }
}

// Asks that the signal handler thread should write an event::Data object into
// |fd| each time a |signo| signal arrives.
//
// Bootstraps the signal handler thread iff it has not yet been set up.
static base::Result sig_tee_add(int fd, int signo) {
  auto lock = base::acquire_lock(g_sig_mu);

  // Bootstrap the signal handler thread, if needed.
  if (!g_sig_tee) {
    Pipe pipe;
    auto result = make_pipe(&pipe);
    if (!result.ok()) return result;
    g_sig_pipe_rfd = pipe.read_fd;
    g_sig_pipe_wfd = pipe.write_fd;
    g_sig_tee = new std::unordered_map<int, std::vector<int>>;
    std::thread(signal_thread_body).detach();
  }

  // Register the signal handler for |signo|, if needed.
  auto& vec = (*g_sig_tee)[signo];
  if (vec.empty()) {
    struct sigaction sa;
    ::bzero(&sa, sizeof(sa));
    sa.sa_sigaction = sigaction_handler;
    sa.sa_flags = SA_SIGINFO;
    int rc = ::sigaction(signo, &sa, nullptr);
    if (rc != 0) {
      int err_no = errno;
      return base::Result::from_errno(err_no, "sigaction(2)");
    }
  }

  // Add |fd| to the tee list for |signo|.
  vec.push_back(fd);
  return base::Result();
}

// Asks that the signal handler thread stop sending |signo| signals to |fd|.
static base::Result sig_tee_remove(int fd, int signo) noexcept {
  auto lock = base::acquire_lock(g_sig_mu);

  if (!g_sig_tee) return base::Result::not_found();

  // Remove |fd| from the tee list for |signo|.
  auto& vec = (*g_sig_tee)[signo];
  vec_erase_all(vec, fd);

  // Unregister the signal handler for |signo|, if needed.
  if (vec.empty()) {
    g_sig_tee->erase(signo);
    struct sigaction sa;
    ::bzero(&sa, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    int rc = ::sigaction(signo, &sa, nullptr);
    if (rc != 0) {
      int err_no = errno;
      return base::Result::from_errno(err_no, "sigaction(2)");
    }
  }

  // The signal handler thread never exits by design.

  return base::Result();
}

// Asks that the signal handler thread stop sending ANY signals to |fd|.
static void sig_tee_remove_all(int fd) noexcept {
  auto lock = base::acquire_lock(g_sig_mu);

  if (!g_sig_tee) return;

  // Walk the unordered_map: for each signal...
  auto it = g_sig_tee->begin(), end = g_sig_tee->end();
  while (it != end) {
    // ... erase |fd| from that signal's tee list.
    const int signo = it->first;
    auto& vec = it->second;
    vec_erase_all(vec, fd);

    // And if the tee list is empty, unregister the signal handler.
    auto old = it;
    ++it;
    if (vec.empty()) {
      g_sig_tee->erase(old);  // Whee, STL iterators are fun.
      struct sigaction sa;
      ::bzero(&sa, sizeof(sa));
      sa.sa_handler = SIG_DFL;
      ::sigaction(signo, &sa, nullptr);
    }
  }
}

// }}}

}  // anonymous namespace

namespace event {

class ManagerImpl {
 public:
  ManagerImpl(std::unique_ptr<Poller> p, std::shared_ptr<Dispatcher> d,
              Pipe pipe, std::size_t min_pollers, std::size_t max_pollers);
  ~ManagerImpl() noexcept;

  Poller& poller() noexcept { return *p_; }
  const Poller& poller() const noexcept { return *p_; }
  Dispatcher& dispatcher() noexcept { return *d_; }
  const Dispatcher& dispatcher() const noexcept { return *d_; }

  base::Result fd_add(base::token_t* out, int fd, Set set,
                      std::shared_ptr<Handler> handler);
  base::Result fd_get(Set* out, int fd, base::token_t t);
  base::Result fd_modify(int fd, base::token_t t, Set set);
  base::Result fd_remove(int fd, base::token_t t);

  base::Result signal_add(base::token_t* out, int signo,
                          std::shared_ptr<Handler> handler);
  base::Result signal_remove(int signo, base::token_t t);

  base::Result timer_add(base::token_t* out, std::shared_ptr<Handler> handler);
  base::Result timer_arm(base::token_t t, base::Duration delay,
                         base::Duration period, bool delay_abs);
  base::Result timer_remove(base::token_t t);

  base::Result generic_add(base::token_t* out,
                           std::shared_ptr<Handler> handler);
  base::Result generic_fire(base::token_t t, int value);
  base::Result generic_remove(base::token_t t);

  base::Result donate(bool forever);

  base::Result shutdown() noexcept;

 private:
  enum class Type : uint8_t {
    undefined = 0,
    fd = 1,
    signal = 2,
    timer = 3,
    generic = 4,
  };

  struct Record {
    std::shared_ptr<Handler> h;
    int n;
    Type type;
    Set set;

    Record(Type type, int n, Set set, std::shared_ptr<Handler> h) noexcept
        : h(std::move(h)),
          n(n),
          type(type),
          set(set) {}

    Record() noexcept : Record(Type::undefined, -1, Set(), nullptr) {}
  };

  base::Result donate_as_poller(base::Lock lock, bool forever);
  base::Result donate_as_mixed(base::Lock lock, bool forever);
  base::Result donate_as_worker(base::Lock lock, bool forever);

  void handle_event(CallbackVec* cbvec, int fd, Set set);
  void handle_pipe_event(CallbackVec* cbvec);
  void handle_timer_event(CallbackVec* cbvec, int fd, base::token_t t);
  void handle_fd_event(CallbackVec* cbvec, int fd, Set set,
                       const std::vector<base::token_t>& t);

  const std::unique_ptr<Poller> p_;
  const std::shared_ptr<Dispatcher> d_;
  mutable std::mutex mu_;
  std::condition_variable curr_cv_;
  std::unordered_map<int, std::vector<base::token_t>> fds_;
  std::unordered_map<int, std::vector<base::token_t>> signals_;
  std::unordered_map<int, base::token_t> timers_;
  std::unordered_map<base::token_t, Record> records_;
  std::size_t min_;
  std::size_t max_;
  std::size_t current_;
  Pipe pipe_;
  bool running_;
};

ManagerImpl::ManagerImpl(std::unique_ptr<Poller> p,
                         std::shared_ptr<Dispatcher> d, Pipe pipe,
                         std::size_t min_pollers, std::size_t max_pollers)
    : p_(std::move(p)),
      d_(std::move(d)),
      min_(min_pollers),
      max_(max_pollers),
      current_(0),
      pipe_(pipe),
      running_(true) {
  auto lock = base::acquire_lock(mu_);
  auto closure = [this] { donate(true).ignore_ok(); };
  for (std::size_t i = 0; i < min_; ++i) {
    std::thread(closure).detach();
  }
  while (current_ < min_) curr_cv_.wait(lock);
}

ManagerImpl::~ManagerImpl() noexcept { shutdown().ignore_ok(); }

base::Result ManagerImpl::fd_add(base::token_t* out, int fd, Set set,
                                 std::shared_ptr<Handler> handler) {
  *out = base::token_t();

  auto lock = base::acquire_lock(mu_);
  base::token_t t = base::next_token();

  records_[t] = Record(Type::fd, fd, set, std::move(handler));
  auto cleanup0 = base::cleanup([this, t] { records_.erase(t); });

  auto fdit = fds_.find(fd);
  bool is_new = false;
  if (fdit == fds_.end()) {
    fdit = fds_.emplace(std::piecewise_construct, std::make_tuple(fd),
                        std::make_tuple())
               .first;
    is_new = true;
  }
  fdit->second.push_back(t);
  auto cleanup1 = base::cleanup([this, fdit] {
    fdit->second.pop_back();
    if (fdit->second.empty()) fds_.erase(fdit);
  });

  base::Result result;
  if (is_new) {
    result = p_->add(fd, set);
  } else {
    Set before;
    for (const auto othertoken : fdit->second) {
      if (othertoken != t) {
        auto it = records_.find(othertoken);
        if (it == records_.end()) continue;
        before |= it->second.set;
      }
    }
    Set after = before | set;
    if (before != after) {
      result = p_->modify(fd, after);
    }
  }
  if (!result.ok()) return result;
  cleanup1.cancel();
  cleanup0.cancel();
  *out = t;
  return base::Result();
}

base::Result ManagerImpl::fd_get(Set* out, int fd, base::token_t t) {
  out->clear();

  auto lock = base::acquire_lock(mu_);
  auto it = records_.find(t);
  if (it == records_.end()) return base::Result::not_found();
  if (it->second.type != Type::fd) return base::Result::wrong_type();
  if (it->second.n != fd) return base::Result::invalid_argument("wrong fd");
  *out = it->second.set;
  return base::Result();
}

base::Result ManagerImpl::fd_modify(int fd, base::token_t t, Set set) {
  auto lock = base::acquire_lock(mu_);

  auto it = records_.find(t);
  if (it == records_.end()) return base::Result::not_found();
  if (it->second.type != Type::fd) return base::Result::wrong_type();
  if (it->second.n != fd) return base::Result::invalid_argument("wrong fd");
  auto& rec = it->second;

  auto fdit = fds_.find(fd);
  if (fdit == fds_.end()) return base::Result::not_found();

  auto& vec = fdit->second;
  Set before, after;
  for (const auto othertoken : vec) {
    if (othertoken == t) {
      before |= rec.set;
      after |= set;
    } else {
      auto rit = records_.find(othertoken);
      if (rit == records_.end()) continue;
      before |= rit->second.set;
      after |= rit->second.set;
    }
  }

  base::Result result;
  if (before != after) {
    result = p_->modify(fd, after);
  }
  if (result.ok()) rec.set = set;
  return result;
}

base::Result ManagerImpl::fd_remove(int fd, base::token_t t) {
  auto lock = base::acquire_lock(mu_);

  auto it = records_.find(t);
  if (it == records_.end()) return base::Result::not_found();
  if (it->second.type != Type::fd) return base::Result::wrong_type();
  if (it->second.n != fd) return base::Result::invalid_argument("wrong fd");
  auto rec = std::move(it->second);
  records_.erase(it);

  auto fdit = fds_.find(fd);
  if (fdit == fds_.end()) return base::Result::not_found();

  auto& vec = fdit->second;
  auto vit = vec.begin();
  Set before, after;
  while (vit != vec.end()) {
    if (*vit == t) {
      before |= rec.set;
      vec.erase(vit);
    } else {
      auto rit = records_.find(*vit);
      if (rit != records_.end()) {
        before |= rit->second.set;
        after |= rit->second.set;
      }
      ++vit;
    }
  }

  base::Result result;
  if (vec.empty()) {
    fds_.erase(fdit);
    result = p_->remove(fd);
  } else if (before != after) {
    result = p_->modify(fd, after);
  }
  return result;
}

base::Result ManagerImpl::signal_add(base::token_t* out, int signo,
                                     std::shared_ptr<Handler> handler) {
  *out = base::token_t();
  assert_valid_signo(signo);

  auto lock = base::acquire_lock(mu_);
  base::token_t t = base::next_token();

  records_[t] = Record(Type::signal, signo, Set(), std::move(handler));
  auto cleanup0 = base::cleanup([this, t] { records_.erase(t); });

  auto sit = signals_.find(signo);
  if (sit == signals_.end()) {
    sit = signals_
              .emplace(std::piecewise_construct, std::make_tuple(signo),
                       std::make_tuple())
              .first;
  }
  auto& vec = sit->second;
  bool was_empty = vec.empty();
  vec.push_back(t);
  auto cleanup1 = base::cleanup([this, sit, &vec] {
    vec.pop_back();
    if (vec.empty()) signals_.erase(sit);
  });

  if (was_empty) {
    auto result = sig_tee_add(pipe_.write_fd, signo);
    if (!result.ok()) return result;
  }

  cleanup0.cancel();
  cleanup1.cancel();
  *out = t;
  return base::Result();
}

base::Result ManagerImpl::signal_remove(int signo, base::token_t t) {
  assert_valid_signo(signo);

  auto lock = base::acquire_lock(mu_);
  auto it = records_.find(t);
  if (it == records_.end()) return base::Result::not_found();
  if (it->second.type != Type::signal) return base::Result::wrong_type();
  if (it->second.n != signo)
    return base::Result::invalid_argument("wrong signal");
  auto rec = std::move(it->second);
  records_.erase(it);

  auto sit = signals_.find(signo);
  if (sit == signals_.end()) return base::Result::not_found();

  auto& vec = sit->second;
  auto vit = vec.begin();
  while (vit != vec.end()) {
    if (*vit == t) {
      vec.erase(vit);
    } else {
      ++vit;
    }
  }

  base::Result result;
  if (vec.empty()) {
    result = sig_tee_remove(pipe_.write_fd, signo);
    signals_.erase(sit);
  }
  return result;
}

base::Result ManagerImpl::timer_add(base::token_t* out,
                                    std::shared_ptr<Handler> handler) {
  *out = base::token_t();
  auto lock = base::acquire_lock(mu_);
  base::token_t t = base::next_token();

  int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
  if (fd == -1) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "timerfd_create(2)");
  }
  auto cleanup0 = base::cleanup([fd] { ::close(fd); });

  records_[t] = Record(Type::timer, fd, Set(), std::move(handler));
  timers_[fd] = t;
  auto cleanup1 = base::cleanup([this, t, fd] {
    timers_.erase(fd);
    records_.erase(t);
  });

  auto result = p_->add(fd, Set::readable_bit());
  if (!result.ok()) return result;

  *out = t;
  cleanup1.cancel();
  cleanup0.cancel();
  return base::Result();
}

base::Result ManagerImpl::timer_arm(base::token_t t, base::Duration delay,
                                    base::Duration period, bool delay_abs) {
  auto lock = base::acquire_lock(mu_);
  auto it = records_.find(t);
  if (it == records_.end()) return base::Result::not_found();
  if (it->second.type != Type::timer) return base::Result::wrong_type();
  auto& rec = it->second;

  int flags = 0;
  if (delay_abs) flags |= TFD_TIMER_ABSTIME;

  struct itimerspec its;
  ::bzero(&its, sizeof(its));
  its.it_value.tv_sec = std::get<1>(delay.raw());
  its.it_value.tv_nsec = std::get<2>(delay.raw());
  its.it_interval.tv_sec = std::get<1>(period.raw());
  its.it_interval.tv_nsec = std::get<2>(period.raw());

  int rc = ::timerfd_settime(rec.n, flags, &its, nullptr);
  if (rc != 0) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "timerfd_settime(2)");
  }
  return base::Result();
}

base::Result ManagerImpl::timer_remove(base::token_t t) {
  auto lock = base::acquire_lock(mu_);
  auto it = records_.find(t);
  if (it == records_.end()) return base::Result::not_found();
  if (it->second.type != Type::timer) return base::Result::wrong_type();
  auto rec = std::move(it->second);
  records_.erase(it);
  timers_.erase(rec.n);

  int rc = ::close(rec.n);
  if (rc != 0) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "close(2)");
  }
  return base::Result();
}

base::Result ManagerImpl::generic_add(base::token_t* out,
                                      std::shared_ptr<Handler> handler) {
  *out = base::token_t();

  auto lock = base::acquire_lock(mu_);
  base::token_t t = base::next_token();
  records_[t] = Record(Type::generic, -1, Set(), std::move(handler));
  *out = t;
  return base::Result();
}

base::Result ManagerImpl::generic_fire(base::token_t t, int value) {
  Data data;
  data.token = t;
  data.int_value = value;
  data.events = Set::event_bit();
  auto lock = base::acquire_lock(mu_);
  auto it = records_.find(t);
  if (it == records_.end()) return base::Result::not_found();
  if (it->second.type != Type::generic) return base::Result::wrong_type();
  return write_exactly(pipe_.write_fd, &data, sizeof(data), "event pipe");
}

base::Result ManagerImpl::generic_remove(base::token_t t) {
  auto lock = base::acquire_lock(mu_);
  auto it = records_.find(t);
  if (it == records_.end()) return base::Result::not_found();
  if (it->second.type != Type::generic) return base::Result::wrong_type();
  records_.erase(it);
  return base::Result();
}

base::Result ManagerImpl::donate(bool forever) {
  auto lock = base::acquire_lock(mu_);
  if (current_ >= max_) {
    return donate_as_worker(std::move(lock), forever);
  } else if (current_ >= min_) {
    return donate_as_mixed(std::move(lock), forever);
  } else {
    return donate_as_poller(std::move(lock), forever);
  }
}

base::Result ManagerImpl::donate_as_poller(base::Lock lock, bool forever) {
  ++current_;
  curr_cv_.notify_all();
  auto cleanup = base::cleanup([this] {
    --current_;
    curr_cv_.notify_all();
  });

  EventVec vec;
  CallbackVec cbvec;
  base::Result result;
  while (running_) {
    lock.unlock();
    auto reacquire0 = base::cleanup([&lock] { lock.lock(); });
    result = p_->wait(&vec, -1);
    reacquire0.run();

    for (const auto& ev : vec) {
      handle_event(&cbvec, ev.first, ev.second);
    }
    vec.clear();

    lock.unlock();
    auto reacquire1 = base::cleanup([&lock] { lock.lock(); });
    for (auto& cb : cbvec) {
      d_->dispatch(nullptr, std::move(cb));
    }
    cbvec.clear();
    reacquire1.run();

    if (!result.ok()) break;
    if (!forever) break;
  }
  result.expect_ok();
  return base::Result();
}

base::Result ManagerImpl::donate_as_mixed(base::Lock lock, bool forever) {
  auto donate_ok = [](const base::Result& result) {
    return result.ok() || result.code() == base::Result::Code::NOT_IMPLEMENTED;
  };

  ++current_;
  curr_cv_.notify_all();
  auto cleanup = base::cleanup([this] {
    --current_;
    curr_cv_.notify_all();
  });

  EventVec vec;
  CallbackVec cbvec;
  base::Result result;
  while (running_) {
    lock.unlock();
    auto reacquire0 = base::cleanup([&lock] { lock.lock(); });
    result = d_->donate(false);
    reacquire0.run();
    if (!donate_ok(result)) break;

    lock.unlock();
    auto reacquire1 = base::cleanup([&lock] { lock.lock(); });
    result = p_->wait(&vec, 0);
    reacquire1.run();

    for (const auto& ev : vec) {
      handle_event(&cbvec, ev.first, ev.second);
    }
    vec.clear();

    lock.unlock();
    auto reacquire2 = base::cleanup([&lock] { lock.lock(); });
    for (auto& cb : cbvec) {
      d_->dispatch(nullptr, std::move(cb));
    }
    cbvec.clear();
    reacquire2.run();

    if (!result.ok()) break;
    if (!forever) break;
  }
  result.expect_ok();
  return base::Result();
}

base::Result ManagerImpl::donate_as_worker(base::Lock lock, bool forever) {
  lock.unlock();
  return d_->donate(forever);
}

void ManagerImpl::handle_event(CallbackVec* cbvec, int fd, Set set) {
  if (fd == pipe_.read_fd) {
    handle_pipe_event(cbvec);
    return;
  }

  auto timerit = timers_.find(fd);
  if (timerit != timers_.end()) {
    handle_timer_event(cbvec, fd, timerit->second);
    return;
  }

  auto fdit = fds_.find(fd);
  if (fdit != fds_.end()) {
    handle_fd_event(cbvec, fd, set, fdit->second);
    return;
  }

  base::Result::internal(
      "BUG: fell off the end of event::ManagerImpl::handle_event")
      .expect_ok();
}

void ManagerImpl::handle_pipe_event(CallbackVec* cbvec) {
  Data data;
  base::Result result;
  while (true) {
    result = read_exactly(pipe_.read_fd, &data, sizeof(data), "event pipe");
    if (result.errno_value() == EAGAIN || result.errno_value() == EWOULDBLOCK)
      return;
    if (!result.ok()) {
      result.expect_ok();
      return;
    }

    if (data.events.signal()) {
      for (const auto& pair : records_) {
        const base::token_t t = pair.first;
        const auto& rec = pair.second;
        if (rec.type == Type::signal && rec.n == data.signal_number) {
          data.token = t;
          cbvec->push_back(handler_callback(rec.h, data));
        }
      }
    }

    if (data.events.event()) {
      for (const auto& pair : records_) {
        const base::token_t t = pair.first;
        const auto& rec = pair.second;
        if (rec.type == Type::generic && t == data.token) {
          data.token = t;
          cbvec->push_back(handler_callback(rec.h, data));
        }
      }
    }
  }
}

void ManagerImpl::handle_timer_event(CallbackVec* cbvec, int fd,
                                     base::token_t t) {
  static constexpr uint64_t INTMAX = std::numeric_limits<int>::max();

  uint64_t x = 0;
  auto result = read_exactly(fd, &x, sizeof(x), "timerfd");
  result.expect_ok();
  if (x > INTMAX) x = INTMAX;

  auto it = records_.find(t);
  if (it == records_.end()) return;
  const auto& rec = it->second;

  Data data;
  data.token = t;
  data.int_value = x;
  data.events = Set::timer_bit();
  cbvec->push_back(handler_callback(rec.h, data));
}

void ManagerImpl::handle_fd_event(CallbackVec* cbvec, int fd, Set set,
                                  const std::vector<base::token_t>& vec) {
  for (const auto t : vec) {
    auto it = records_.find(t);
    if (it != records_.end()) {
      const auto& rec = it->second;
      Data data;
      data.token = t;
      data.fd = fd;
      data.events = rec.set & set;
      if (data.events) {
        cbvec->push_back(handler_callback(rec.h, data));
      }
    }
  }
}

base::Result ManagerImpl::shutdown() noexcept {
  auto lock = base::acquire_lock(mu_);

  if (!running_) return base::Result::failed_precondition("already stopped");

  // Mark ourselves as no longer running.
  running_ = false;

  // Throw away all handlers and ancilliary data.
  fds_.clear();
  signals_.clear();
  records_.clear();
  sig_tee_remove_all(pipe_.write_fd);

  // Wait for the pollers to notice.
  while (current_ > 0) {
    Data x;
    write_exactly(pipe_.write_fd, &x, sizeof(x), "event pipe").expect_ok();
    curr_cv_.wait(lock);
  }

  // Close the event pipe fds.
  int w_fd = -1, r_fd = -1;
  std::swap(w_fd, pipe_.write_fd);
  std::swap(r_fd, pipe_.read_fd);
  int w_rc = ::close(w_fd);
  int w_errno = errno;
  int r_rc = ::close(r_fd);
  int r_errno = errno;
  d_->shutdown();
  if (w_rc != 0)
    return base::Result::from_errno(w_errno, "close(2)");
  else if (r_rc != 0)
    return base::Result::from_errno(r_errno, "close(2)");
  else
    return base::Result();
}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
  release().ignore_ok();
  swap(other);
  return *this;
}

void FileDescriptor::assert_valid() const {
  if (!valid()) throw std::logic_error("event::FileDescriptor is empty");
}

base::Result FileDescriptor::get(Set* out) const {
  assert_valid();
  return ptr_->fd_get(out, fd_, t_);
}

base::Result FileDescriptor::modify(Set set) {
  assert_valid();
  return ptr_->fd_modify(fd_, t_, set);
}

base::Result FileDescriptor::release() {
  std::shared_ptr<ManagerImpl> ptr;
  ptr.swap(ptr_);
  if (ptr) return ptr->fd_remove(fd_, t_);
  return base::Result();
}

Signal& Signal::operator=(Signal&& other) noexcept {
  release().ignore_ok();
  swap(other);
  return *this;
}

void Signal::assert_valid() const {
  if (!valid()) throw std::logic_error("event::Signal is empty");
}

base::Result Signal::release() {
  std::shared_ptr<ManagerImpl> ptr;
  ptr.swap(ptr_);
  if (ptr) return ptr->signal_remove(sig_, t_);
  return base::Result();
}

Timer& Timer::operator=(Timer&& other) noexcept {
  release().ignore_ok();
  swap(other);
  return *this;
}

void Timer::assert_valid() const {
  if (!valid()) throw std::logic_error("event::Timer is empty");
}

base::Result Timer::set_at(base::MonotonicTime at) {
  assert_valid();
  base::Duration delay = at.since_epoch();
  if (delay.is_zero() || delay.is_neg())
    return base::Result::invalid_argument(
        "initial event must be strictly after the epoch");
  return ptr_->timer_arm(t_, delay, base::Duration(), true);
}

base::Result Timer::set_delay(base::Duration delay) {
  assert_valid();
  if (delay.is_zero() || delay.is_neg())
    return base::Result::invalid_argument(
        "delay must be strictly after the present");
  return ptr_->timer_arm(t_, delay, base::Duration(), false);
}

base::Result Timer::set_periodic(base::Duration period) {
  assert_valid();
  if (period.is_zero() || period.is_neg())
    return base::Result::invalid_argument(
        "zero or negative period doesn't make sense");
  return ptr_->timer_arm(t_, period, period, false);
}

base::Result Timer::set_periodic_at(base::Duration period, base::MonotonicTime at) {
  assert_valid();
  base::Duration delay = at.since_epoch();
  if (period.is_zero() || period.is_neg())
    return base::Result::invalid_argument(
        "zero or negative period doesn't make sense");
  if (delay.is_zero() || delay.is_neg())
    return base::Result::invalid_argument(
        "initial event must be strictly after the epoch");
  return ptr_->timer_arm(t_, delay, period, true);
}

base::Result Timer::set_periodic_delay(base::Duration period,
                                       base::Duration delay) {
  assert_valid();
  if (period.is_zero() || period.is_neg())
    return base::Result::invalid_argument(
        "zero or negative period doesn't make sense");
  if (delay.is_zero() || delay.is_neg())
    return base::Result::invalid_argument(
        "delay must be strictly after the present");
  return ptr_->timer_arm(t_, delay, period, false);
}

base::Result Timer::cancel() {
  assert_valid();
  return ptr_->timer_arm(t_, base::Duration(), base::Duration(), false);
}

base::Result Timer::release() {
  std::shared_ptr<ManagerImpl> ptr;
  ptr.swap(ptr_);
  if (ptr) return ptr->timer_remove(t_);
  return base::Result();
}

Generic& Generic::operator=(Generic&& other) noexcept {
  release().ignore_ok();
  swap(other);
  return *this;
}

void Generic::assert_valid() const {
  if (!valid()) throw std::logic_error("event::Generic is empty");
}

base::Result Generic::fire(int value) const {
  assert_valid();
  return ptr_->generic_fire(t_, value);
}

base::Result Generic::release() {
  std::shared_ptr<ManagerImpl> ptr;
  ptr.swap(ptr_);
  if (ptr) return ptr->generic_remove(t_);
  return base::Result();
}

void Manager::assert_valid() const {
  if (!ptr_) throw std::logic_error("event::Manager is empty");
}

Poller& Manager::poller() const {
  assert_valid();
  return ptr_->poller();
}

Dispatcher& Manager::dispatcher() const {
  assert_valid();
  return ptr_->dispatcher();
}

base::Result Manager::fd(FileDescriptor* out, int fd, Set set,
                         std::shared_ptr<Handler> handler) const {
  base::Result result = out->release();
  if (result.ok()) {
    assert_valid();
    base::token_t t;
    result = ptr_->fd_add(&t, fd, set, std::move(handler));
    if (result.ok()) {
      *out = FileDescriptor(ptr_, fd, t);
    }
  }
  return result;
}

base::Result Manager::signal(Signal* out, int signo,
                             std::shared_ptr<Handler> handler) const {
  base::Result result = out->release();
  if (result.ok()) {
    assert_valid();
    base::token_t t;
    result = ptr_->signal_add(&t, signo, std::move(handler));
    if (result.ok()) {
      *out = Signal(ptr_, signo, t);
    }
  }
  return result;
}

base::Result Manager::timer(Timer* out,
                            std::shared_ptr<Handler> handler) const {
  base::Result result = out->release();
  if (result.ok()) {
    assert_valid();
    base::token_t t;
    result = ptr_->timer_add(&t, std::move(handler));
    if (result.ok()) {
      *out = Timer(ptr_, t);
    }
  }
  return result;
}

base::Result Manager::generic(Generic* out,
                              std::shared_ptr<Handler> handler) const {
  base::Result result = out->release();
  if (result.ok()) {
    assert_valid();
    base::token_t t;
    result = ptr_->generic_add(&t, std::move(handler));
    if (result.ok()) {
      *out = Generic(ptr_, t);
    }
  }
  return result;
}

base::Result Manager::set_deadline(Task* task, base::MonotonicTime at) {
  auto* t = new Timer;
  auto closure0 = [t] {
    delete t;
    return base::Result();
  };
  auto closure1 = [task](Data unused) {
    task->expire();
    return base::Result();
  };
  task->on_finished(callback(closure0));
  auto r = timer(t, handler(closure1));
  if (r) r = t->set_at(at);
  return r;
}

base::Result Manager::set_timeout(Task* task, base::Duration delay) {
  auto* t = new Timer;
  auto closure0 = [t] {
    delete t;
    return base::Result();
  };
  auto closure1 = [task](Data unused) {
    task->expire();
    return base::Result();
  };
  task->on_finished(callback(closure0));
  auto r = timer(t, handler(closure1));
  if (r) r = t->set_delay(delay);
  return r;
}

base::Result Manager::donate(bool forever) const {
  assert_valid();
  return ptr_->donate(forever);
}

base::Result Manager::shutdown() const {
  assert_valid();
  return ptr_->shutdown();
}

namespace {
struct WaitData {
  std::mutex mu;
  std::condition_variable cv;
  std::size_t done;

  WaitData() noexcept : done(0) {}
};
}  // anonymous namespace

void wait_n(std::vector<Manager> mv, std::vector<Task*> tv, std::size_t n) {
  if (n > tv.size())
    throw std::logic_error(
        "event::wait_n asked to wait for more task "
        "completions than there are provided tasks");

  auto closure = [](std::shared_ptr<WaitData> data) {
    auto lock = base::acquire_lock(data->mu);
    ++data->done;
    data->cv.notify_all();
    return base::Result();
  };

  auto data = std::make_shared<WaitData>();
  for (Task* task : tv) {
    task->on_finished(callback(closure, data));
  }

  bool any_threaded = false;
  for (const Manager& m : mv) {
    if (m.dispatcher().type() == DispatcherType::threaded_dispatcher) {
      any_threaded = true;
      break;
    }
  }

  auto lock = base::acquire_lock(data->mu);
  while (data->done < n) {
    // Inline? Maybe it's blocked on I/O. Try donating.
    // Async? Just donate.
    // Threaded? Don't be so eager to join the fray.
    if (any_threaded) {
      using MS = std::chrono::milliseconds;
      data->cv.wait_for(lock, MS(1));
      if (data->done >= n) return;
    }
    lock.unlock();
    for (const Manager& m : mv) {
      m.donate(false).assert_ok();
    }
    lock.lock();
  }
}

static base::Result make_manager(std::shared_ptr<ManagerImpl>* out,
                                 const ManagerOptions& o) {
  auto min = o.min_pollers();
  auto max = o.max_pollers();
  if (!min.first) min.second = 1;
  if (!max.first) max.second = min.second;
  if (min.second > max.second)
    return base::Result::invalid_argument("min_pollers > max_pollers");
  if (max.second < 1) return base::Result::invalid_argument("max_pollers < 1");

  Pipe pipe;
  base::Result result = make_pipe(&pipe);
  if (!result.ok()) return result;
  auto cleanup = base::cleanup([pipe] {
    ::close(pipe.write_fd);
    ::close(pipe.read_fd);
  });

  int flags = ::fcntl(pipe.read_fd, F_GETFL);
  if (flags == -1) {
    int err_no = errno;
    result = base::Result::from_errno(err_no, "fcntl(2)");
    return result;
  }
  flags = ::fcntl(pipe.read_fd, F_SETFL, flags | O_NONBLOCK);
  if (flags == -1) {
    int err_no = errno;
    result = base::Result::from_errno(err_no, "fcntl(2)");
    return result;
  }

  std::unique_ptr<Poller> p;
  result = new_poller(&p, o.poller());
  if (!result.ok()) return result;

  result = p->add(pipe.read_fd, Set::readable_bit());
  if (!result.ok()) return result;

  std::shared_ptr<Dispatcher> d;
  result = new_dispatcher(&d, o.dispatcher());
  if (!result.ok()) return result;

  *out = std::make_shared<ManagerImpl>(std::move(p), std::move(d), pipe,
                                       min.second, max.second);
  cleanup.cancel();
  return result;
}

base::Result new_manager(Manager* out, const ManagerOptions& o) {
  out->reset();
  std::shared_ptr<ManagerImpl> ptr;
  auto result = make_manager(&ptr, o);
  if (result.ok()) *out = Manager(std::move(ptr));
  return result;
}

static std::mutex g_sysmgr_mu;

static Manager* g_sysmgr_ptr = nullptr;

Manager& system_manager() {
  auto lock = base::acquire_lock(g_sysmgr_mu);
  if (g_sysmgr_ptr == nullptr) {
    ManagerOptions o;
    std::unique_ptr<Manager> m(new Manager);
    new_manager(m.get(), o).assert_ok();
    g_sysmgr_ptr = m.release();
  }
  return *g_sysmgr_ptr;
}

void set_system_manager(Manager m) {
  auto lock = base::acquire_lock(g_sysmgr_mu);
  if (g_sysmgr_ptr == nullptr) {
    g_sysmgr_ptr = new Manager(std::move(m));
  } else {
    *g_sysmgr_ptr = std::move(m);
  }
}

}  // namespace event
