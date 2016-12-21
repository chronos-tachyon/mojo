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
#include "base/logging.h"
#include "base/util.h"

using TeeVec = std::vector<base::FD>;
using TeeMap = std::unordered_map<int, TeeVec>;
using CallbackVec = std::vector<event::CallbackPtr>;
using base::token_t;

namespace event {

namespace {

static int get_fdnum(const base::FD& fd) {
  auto pair = fd->acquire_fd();
  return pair.first;
}

static bool donate_ok(const base::Result& r) {
  return r || r.code() == base::Result::Code::NOT_IMPLEMENTED;
}

template <typename T, typename... Args>
static std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <typename T>
static bool vec_erase_all(std::vector<T>& vec, const T& item) noexcept {
  bool found = false;
  auto begin = vec.begin(), it = vec.end();
  while (it != begin) {
    --it;
    if (*it == item) {
      auto tmp = it - 1;
      vec.erase(it);
      it = tmp + 1;
      found = true;
    }
  }
  return found;
}

// Signal handler implementation details {{{

static constexpr std::size_t NUM_SIGNALS =
#ifdef _NSIG
    _NSIG
#else
    64  // reasonable guess
#endif
    ;

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

// Guards: g_sig_pipe_rfd, g_sig_pipe_wfd, g_sig_tee
static std::mutex g_sig_mu;

// The signal handler pipe.
// Only the signal handler itself should write to this pipe.
// Only the background thread should read from this pipe.
// THREAD SAFETY NOTE: these values MUST remain constant after initialization
static base::FD* g_sig_pipe_rfd = nullptr;
static int g_sig_pipe_wfd = -1;

// FDs interested in receiving signals, arranged by signal number.
// When the background thread reads an event, it will tee into these FDs.
static TeeMap* g_sig_tee = nullptr;

// This is the actual signal handler.
// Happily, write(2) is safe to call from within a signal handler.
// Sadly, we can't do much error checking.
static void sigaction_handler(int signo, siginfo_t* si, void* context) {
  ::write(g_sig_pipe_wfd, si, sizeof(*si));
}

// This thread services the read end of the signal handler pipe.
static void signal_thread_body() {
  base::Lock lock(g_sig_mu, std::defer_lock);
  TeeVec vec;
  siginfo_t si;
  base::Result r;
  while (true) {
    r = base::read_exactly(*g_sig_pipe_rfd, &si, sizeof(si), "signal pipe");
    if (r.code() == base::Result::Code::END_OF_FILE) break;
    r.expect_ok(__FILE__, __LINE__);
    if (!r) continue;

    lock.lock();
    auto it = g_sig_tee->find(si.si_signo);
    if (it == g_sig_tee->end())
      vec.clear();
    else
      vec = it->second;
    lock.unlock();

    event::Data data;
    populate_data_from_siginfo(&data, si);
    for (const auto& fd : vec) {
      r = base::write_exactly(fd, &data, sizeof(data), "pipe");
      r.expect_ok(__FILE__, __LINE__);
    }
  }
}

// Asks that the signal handler thread should write an event::Data object into
// |fd| each time a |signo| signal arrives.
//
// Bootstraps the signal handler thread iff it has not yet been set up.
static base::Result sig_tee_add(base::FD fd, int signo) {
  auto lock = base::acquire_lock(g_sig_mu);

  // Bootstrap the signal handler thread, if needed.
  if (!g_sig_pipe_rfd) {
    base::Pipe pipe;
    base::Result r = base::make_pipe(&pipe);
    if (!r) return r;
    r = base::set_blocking(pipe.read, true);
    if (!r) return r;
    g_sig_pipe_rfd = new base::FD(std::move(pipe.read));
    g_sig_pipe_wfd = pipe.write->release_fd();
  }
  if (!g_sig_tee) {
    g_sig_tee = new TeeMap;
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
  vec.push_back(std::move(fd));
  return base::Result();
}

// Asks that the signal handler thread stop sending |signo| signals to |fd|.
static base::Result sig_tee_remove(base::FD fd, int signo) noexcept {
  auto lock = base::acquire_lock(g_sig_mu);

  if (!g_sig_tee) return base::Result::not_found();

  // Remove |fd| from the tee list for |signo|.
  auto& vec = (*g_sig_tee)[signo];
  if (!vec_erase_all(vec, fd)) return base::Result::not_found();

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
static void sig_tee_remove_all(base::FD fd) noexcept {
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
    auto old = it;  // Whee, STL iterators are fun.
    ++it;
    if (vec.empty()) {
      g_sig_tee->erase(old);
      struct sigaction sa;
      ::bzero(&sa, sizeof(sa));
      sa.sa_handler = SIG_DFL;
      ::sigaction(signo, &sa, nullptr);
    }
  }
}

// }}}
// Record definition {{{

enum class SourceType : uint8_t {
  undefined = 0,
  fd = 1,
  signal = 2,
  timer = 3,
  generic = 4,
};

struct Record {
  mutable std::mutex mu;       // protects outstanding
  std::condition_variable cv;  // notified when outstanding == 0
  unsigned int outstanding;    // # of pending event handler invocations
  token_t t;
  HandlerPtr h;
  Set set;

  Record(token_t t, Set set, HandlerPtr h) noexcept : outstanding(0),
                                                      t(t),
                                                      h(std::move(h)),
                                                      set(set) {}
};

using RecordPtr = std::unique_ptr<Record>;

struct Source {
  std::vector<RecordPtr> records;
  base::FD fd;
  int signo;
  token_t gt;
  SourceType type;

  Source() noexcept : signo(0), type(SourceType::undefined) {}
};

// }}}
// HandlerCallback implementation {{{

class HandlerCallback : public Callback {
 public:
  HandlerCallback(Record* rec, Data data) noexcept : rec_(rec),
                                                     data_(std::move(data)) {
    auto lock = base::acquire_lock(rec_->mu);
    ++rec_->outstanding;
  }

  base::Result run() override {
    auto cleanup = base::cleanup([this] {
      auto lock = base::acquire_lock(rec_->mu);
      --rec_->outstanding;
      if (!rec_->outstanding) rec_->cv.notify_all();
    });
    return rec_->h->run(data_);
  }

 private:
  Record* rec_;
  Data data_;
};

static CallbackPtr handler_callback(Record* rec, Data data) {
  return CallbackPtr(new HandlerCallback(rec, std::move(data)));
}

// }}}

}  // anonymous namespace

class ManagerImpl {
 public:
  ManagerImpl(PollerPtr p, DispatcherPtr d, base::Pipe pipe,
              std::size_t min_pollers, std::size_t max_pollers);
  ~ManagerImpl() noexcept;

  bool is_running() const noexcept {
    auto lock = base::acquire_lock(mu_);
    return running_;
  }

  PollerPtr poller() const noexcept {
    auto lock = base::acquire_lock(mu_);
    return DCHECK_NOTNULL(p_);
  }

  DispatcherPtr dispatcher() const noexcept {
    auto lock = base::acquire_lock(mu_);
    return DCHECK_NOTNULL(d_);
  }

  base::Result fd_add(token_t* out, base::FD fd, Set set, HandlerPtr handler);
  base::Result fd_get(Set* out, token_t t);
  base::Result fd_modify(token_t t, Set set);
  base::Result fd_remove(token_t t);

  base::Result signal_add(token_t* out, int signo, HandlerPtr handler);
  base::Result signal_remove(token_t t);

  base::Result timer_add(token_t* out, HandlerPtr handler);
  base::Result timer_arm(token_t t, base::Duration delay, base::Duration period,
                         bool delay_abs);
  base::Result timer_remove(token_t t);

  base::Result generic_add(token_t* out, HandlerPtr handler);
  base::Result generic_fire(token_t t, int value);
  base::Result generic_remove(token_t t);

  base::Result donate(bool forever);

  base::Result shutdown() noexcept;

 private:
  base::Result donate_as_poller(base::Lock lock, bool forever);
  base::Result donate_as_mixed(base::Lock lock, bool forever);
  base::Result donate_as_worker(base::Lock lock, bool forever);

  void handle_event(CallbackVec* cbvec, token_t gt, Set set);
  void handle_pipe_event(CallbackVec* cbvec);
  void handle_timer_event(CallbackVec* cbvec, const Source& src);
  void handle_fd_event(CallbackVec* cbvec, const Source& src, Set set);

  void wait_on_record(base::Lock lock, Record* rec) const;

  mutable std::mutex mu_;
  std::condition_variable curr_cv_;
  std::unordered_map<int, token_t> fdmap_;       // fd -> global token
  std::unordered_map<int, token_t> sigmap_;      // signo -> global token
  std::unordered_map<token_t, token_t> ltmap_;   // local -> global token
  std::unordered_map<token_t, Source> sources_;  // global token -> source
  PollerPtr p_;
  DispatcherPtr d_;
  base::Pipe pipe_;
  std::size_t min_;      // min # of poller threads
  std::size_t max_;      // max # of poller threads
  std::size_t current_;  // current # of poller threads
  bool running_;         // true iff not shut down
};

ManagerImpl::ManagerImpl(PollerPtr p, DispatcherPtr d, base::Pipe pipe,
                         std::size_t min_pollers, std::size_t max_pollers)
    : p_(DCHECK_NOTNULL(std::move(p))),
      d_(DCHECK_NOTNULL(std::move(d))),
      pipe_(std::move(pipe)),
      min_(min_pollers),
      max_(max_pollers),
      current_(0),
      running_(true) {
  DCHECK_NOTNULL(pipe_.write);
  DCHECK_NOTNULL(pipe_.read);
  auto lock = base::acquire_lock(mu_);
  auto closure = [this] { donate(true).ignore_ok(); };
  for (std::size_t i = 0; i < min_; ++i) {
    std::thread(closure).detach();
  }
  while (current_ < min_) curr_cv_.wait(lock);
}

ManagerImpl::~ManagerImpl() noexcept {
  shutdown().expect_ok(__FILE__, __LINE__);
}

base::Result ManagerImpl::fd_add(base::token_t* out, base::FD fd, Set set,
                                 HandlerPtr handler) {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(fd);
  DCHECK_NOTNULL(handler);

  *out = token_t();
  const int fdnum = get_fdnum(fd);
  if (fdnum == -1)
    return base::Result::invalid_argument("file descriptor is closed");

  auto lock = base::acquire_lock(mu_);
  PollerPtr p = DCHECK_NOTNULL(p_);

  token_t gt;
  bool added_gt = false;
  auto fdit = fdmap_.find(fdnum);
  if (fdit == fdmap_.end()) {
    gt = base::next_token();
    fdmap_[fdnum] = gt;
    added_gt = true;
  } else {
    gt = fdit->second;
  }
  auto cleanup0 = base::cleanup([this, fdnum, added_gt] {
    if (added_gt) fdmap_.erase(fdnum);
  });

  token_t t = base::next_token();
  ltmap_[t] = gt;
  auto cleanup1 = base::cleanup([this, t] { ltmap_.erase(t); });

  auto& src = sources_[gt];
  if (src.records.empty()) {
    src.type = SourceType::fd;
    src.fd = std::move(fd);
    src.gt = gt;
  } else {
    fd = nullptr;
  }
  src.records.push_back(make_unique<Record>(t, set, std::move(handler)));
  auto cleanup2 = base::cleanup([this, gt, &src] {
    src.records.pop_back();
    if (src.records.empty()) sources_.erase(gt);
  });

  base::Result r;
  if (added_gt) {
    r = p->add(src.fd, gt, set);
  } else {
    Set before;
    for (const auto& rec : src.records) {
      if (rec->t != t) before |= rec->set;
    }
    Set after = before | set;
    if (before != after) {
      r = p->modify(src.fd, gt, after);
    }
  }
  if (r) {
    *out = t;
    cleanup2.cancel();
    cleanup1.cancel();
    cleanup0.cancel();
  }
  return r;
}

base::Result ManagerImpl::fd_get(Set* out, base::token_t t) {
  DCHECK_NOTNULL(out)->clear();

  auto lock = base::acquire_lock(mu_);
  auto ltit = ltmap_.find(t);
  if (ltit == ltmap_.end()) return base::Result::not_found();
  auto gt = ltit->second;

  auto srcit = sources_.find(gt);
  if (srcit == sources_.end()) return base::Result::not_found();
  if (srcit->second.type != SourceType::fd) return base::Result::wrong_type();
  const auto& src = srcit->second;

  for (const auto& rec : src.records) {
    if (rec->t == t) {
      *out = rec->set;
      return base::Result();
    }
  }
  return base::Result::not_found();
}

base::Result ManagerImpl::fd_modify(base::token_t t, Set set) {
  auto lock = base::acquire_lock(mu_);
  PollerPtr p = DCHECK_NOTNULL(p_);

  auto ltit = ltmap_.find(t);
  if (ltit == ltmap_.end()) return base::Result::not_found();
  auto gt = ltit->second;

  auto srcit = sources_.find(gt);
  if (srcit == sources_.end()) return base::Result::not_found();
  if (srcit->second.type != SourceType::fd) return base::Result::wrong_type();
  auto& src = srcit->second;

  Record* myrec = nullptr;
  Set before, after;
  for (const auto& rec : src.records) {
    before |= rec->set;
    if (rec->t == t) {
      myrec = rec.get();
      after |= set;
    } else {
      after |= rec->set;
    }
  }
  if (myrec == nullptr) return base::Result::not_found();

  base::Result r;
  if (before != after) {
    r = p->modify(src.fd, gt, after);
  }
  if (r) myrec->set = set;
  return r;
}

base::Result ManagerImpl::fd_remove(base::token_t t) {
  auto lock = base::acquire_lock(mu_);
  if (!running_) return base::Result();
  PollerPtr p = DCHECK_NOTNULL(p_);

  auto ltit = ltmap_.find(t);
  if (ltit == ltmap_.end()) return base::Result::not_found();
  auto gt = ltit->second;

  auto srcit = sources_.find(gt);
  if (srcit == sources_.end()) return base::Result::not_found();
  if (srcit->second.type != SourceType::fd) return base::Result::wrong_type();
  auto& src = srcit->second;

  RecordPtr myrec;
  Set before, after;
  auto rit = src.records.begin();
  while (rit != src.records.end()) {
    auto& rec = *rit;
    before |= rec->set;
    if (rec->t == t) {
      myrec = std::move(rec);
      src.records.erase(rit);
    } else {
      after |= rec->set;
      ++rit;
    }
  }
  if (!myrec) return base::Result::not_found();

  base::Result r;
  ltmap_.erase(t);
  if (src.records.empty()) {
    auto fd = std::move(src.fd);
    const int fdnum = get_fdnum(fd);
    sources_.erase(gt);
    fdmap_.erase(fdnum);
    r = p->remove(fd);
  } else if (before != after) {
    r = p->modify(src.fd, gt, after);
  }

  wait_on_record(std::move(lock), myrec.get());

  return r;
}

base::Result ManagerImpl::signal_add(base::token_t* out, int signo,
                                     HandlerPtr handler) {
  *DCHECK_NOTNULL(out) = token_t();
  DCHECK_NOTNULL(handler);

  if (signo < 0 || std::size_t(signo) >= NUM_SIGNALS) {
    return base::Result::invalid_argument("invalid signal number ", signo);
  }

  auto lock = base::acquire_lock(mu_);

  token_t gt;
  bool added_gt = false;
  auto sigit = sigmap_.find(signo);
  if (sigit == sigmap_.end()) {
    gt = base::next_token();
    sigmap_[signo] = gt;
    added_gt = true;
  } else {
    gt = sigit->second;
  }
  auto cleanup0 = base::cleanup([this, signo, added_gt] {
    if (added_gt) sigmap_.erase(signo);
  });

  token_t t = base::next_token();
  ltmap_[t] = gt;
  auto cleanup1 = base::cleanup([this, t] { ltmap_.erase(t); });

  auto& src = sources_[gt];
  if (src.records.empty()) {
    src.type = SourceType::signal;
    src.signo = signo;
    src.gt = gt;
  }
  src.records.push_back(make_unique<Record>(t, Set(), std::move(handler)));
  auto cleanup2 = base::cleanup([this, gt, &src] {
    src.records.pop_back();
    if (src.records.empty()) sources_.erase(gt);
  });

  base::Result r;
  if (added_gt) {
    r = sig_tee_add(pipe_.write, signo);
  }
  if (r) {
    *out = t;
    cleanup2.cancel();
    cleanup1.cancel();
    cleanup0.cancel();
  }
  return r;
}

base::Result ManagerImpl::signal_remove(base::token_t t) {
  auto lock = base::acquire_lock(mu_);
  if (!running_) return base::Result();

  auto ltit = ltmap_.find(t);
  if (ltit == ltmap_.end()) return base::Result::not_found();
  auto gt = ltit->second;

  auto srcit = sources_.find(gt);
  if (srcit == sources_.end()) return base::Result::not_found();
  if (srcit->second.type != SourceType::signal)
    return base::Result::wrong_type();
  auto& src = srcit->second;

  RecordPtr myrec;
  auto rit = src.records.begin();
  while (rit != src.records.end()) {
    auto& rec = *rit;
    if (rec->t == t) {
      myrec = std::move(rec);
      src.records.erase(rit);
      break;
    }
    ++rit;
  }
  if (!myrec) return base::Result::not_found();

  base::Result r;
  ltmap_.erase(t);
  if (src.records.empty()) {
    int signo = src.signo;
    sigmap_.erase(signo);
    sources_.erase(gt);
    r = sig_tee_remove(pipe_.write, signo);
  }

  wait_on_record(std::move(lock), myrec.get());

  return r;
}

base::Result ManagerImpl::timer_add(base::token_t* out, HandlerPtr handler) {
  *DCHECK_NOTNULL(out) = token_t();
  DCHECK_NOTNULL(handler);

  int fdnum = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
  if (fdnum == -1) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "timerfd_create(2)");
  }
  base::FD fd = base::FDHolder::make(fdnum);

  auto lock = base::acquire_lock(mu_);
  PollerPtr p = DCHECK_NOTNULL(p_);

  token_t t = base::next_token();
  auto& src = sources_[t];
  src.type = SourceType::timer;
  src.fd = std::move(fd);
  src.gt = t;
  src.records.push_back(make_unique<Record>(t, Set(), std::move(handler)));
  auto cleanup = base::cleanup([this, t] { sources_.erase(t); });

  base::Result r = p->add(src.fd, t, Set::readable_bit());
  if (r) {
    *out = t;
    cleanup.cancel();
  }
  return r;
}

base::Result ManagerImpl::timer_arm(base::token_t t, base::Duration delay,
                                    base::Duration period, bool delay_abs) {
  auto lock = base::acquire_lock(mu_);
  auto srcit = sources_.find(t);
  if (srcit == sources_.end()) return base::Result::not_found();
  if (srcit->second.type != SourceType::timer)
    return base::Result::wrong_type();
  auto& src = srcit->second;

  int flags = 0;
  if (delay_abs) flags |= TFD_TIMER_ABSTIME;

  struct itimerspec its;
  ::bzero(&its, sizeof(its));
  its.it_value.tv_sec = std::get<1>(delay.raw());
  its.it_value.tv_nsec = std::get<2>(delay.raw());
  its.it_interval.tv_sec = std::get<1>(period.raw());
  its.it_interval.tv_nsec = std::get<2>(period.raw());

  auto pair = src.fd->acquire_fd();
  int rc = ::timerfd_settime(pair.first, flags, &its, nullptr);
  if (rc != 0) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "timerfd_settime(2)");
  }
  return base::Result();
}

base::Result ManagerImpl::timer_remove(base::token_t t) {
  auto lock = base::acquire_lock(mu_);
  if (!running_) return base::Result();
  PollerPtr p = DCHECK_NOTNULL(p_);

  auto srcit = sources_.find(t);
  if (srcit == sources_.end()) return base::Result::not_found();
  if (srcit->second.type != SourceType::timer)
    return base::Result::wrong_type();
  auto& src = srcit->second;

  CHECK_EQ(src.records.size(), 1U);
  RecordPtr myrec(std::move(src.records.back()));
  base::FD fd(std::move(src.fd));
  sources_.erase(srcit);
  base::Result r0 = p->remove(fd);
  base::Result r1 = fd->close();

  wait_on_record(std::move(lock), myrec.get());

  return r0.and_then(r1);
}

base::Result ManagerImpl::generic_add(base::token_t* out, HandlerPtr handler) {
  *DCHECK_NOTNULL(out) = token_t();
  DCHECK_NOTNULL(handler);

  auto lock = base::acquire_lock(mu_);
  token_t t = base::next_token();
  auto& src = sources_[t];
  src.type = SourceType::generic;
  src.gt = t;
  src.records.push_back(make_unique<Record>(t, Set(), std::move(handler)));
  *out = t;
  return base::Result();
}

base::Result ManagerImpl::generic_fire(base::token_t t, int value) {
  Data data;
  data.token = t;
  data.int_value = value;
  data.events = Set::event_bit();
  auto lock = base::acquire_lock(mu_);
  auto srcit = sources_.find(t);
  if (srcit == sources_.end()) return base::Result::not_found();
  if (srcit->second.type != SourceType::generic)
    return base::Result::wrong_type();
  return base::write_exactly(pipe_.write, &data, sizeof(data), "event pipe");
}

base::Result ManagerImpl::generic_remove(base::token_t t) {
  auto lock = base::acquire_lock(mu_);
  if (!running_) return base::Result();

  auto srcit = sources_.find(t);
  if (srcit == sources_.end()) return base::Result::not_found();
  if (srcit->second.type != SourceType::generic)
    return base::Result::wrong_type();
  auto& src = srcit->second;

  CHECK_EQ(src.records.size(), 1U);
  RecordPtr myrec(std::move(src.records.back()));
  sources_.erase(srcit);

  wait_on_record(std::move(lock), myrec.get());

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

  DispatcherPtr d = DCHECK_NOTNULL(d_);
  PollerPtr p = DCHECK_NOTNULL(p_);

  Poller::EventVec vec;
  CallbackVec cbvec;
  base::Result r;
  while (running_) {
    lock.unlock();
    auto reacquire0 = base::cleanup([&lock] { lock.lock(); });
    r = p->wait(&vec, -1);
    reacquire0.run();

    for (const auto& ev : vec) {
      handle_event(&cbvec, ev.first, ev.second);
    }
    vec.clear();

    lock.unlock();
    auto reacquire1 = base::cleanup([&lock] { lock.lock(); });
    for (auto& cb : cbvec) {
      d->dispatch(nullptr, std::move(cb));
    }
    cbvec.clear();
    reacquire1.run();

    if (!r) break;
    if (!forever) break;
  }
  r.expect_ok(__FILE__, __LINE__);
  return base::Result();
}

base::Result ManagerImpl::donate_as_mixed(base::Lock lock, bool forever) {
  ++current_;
  curr_cv_.notify_all();
  auto cleanup = base::cleanup([this] {
    --current_;
    curr_cv_.notify_all();
  });

  DispatcherPtr d = DCHECK_NOTNULL(d_);
  PollerPtr p = DCHECK_NOTNULL(p_);

  Poller::EventVec vec;
  CallbackVec cbvec;
  base::Result r;
  while (running_) {
    lock.unlock();
    auto reacquire0 = base::cleanup([&lock] { lock.lock(); });
    r = d->donate(false);
    reacquire0.run();
    if (!donate_ok(r)) break;

    lock.unlock();
    auto reacquire1 = base::cleanup([&lock] { lock.lock(); });
    r = p->wait(&vec, 0);
    reacquire1.run();

    for (const auto& ev : vec) {
      handle_event(&cbvec, ev.first, ev.second);
    }
    vec.clear();

    lock.unlock();
    auto reacquire2 = base::cleanup([&lock] { lock.lock(); });
    for (auto& cb : cbvec) {
      d->dispatch(nullptr, std::move(cb));
    }
    cbvec.clear();
    reacquire2.run();

    if (!r) break;
    if (!forever) break;
  }
  r.expect_ok(__FILE__, __LINE__);
  return base::Result();
}

base::Result ManagerImpl::donate_as_worker(base::Lock lock, bool forever) {
  DispatcherPtr d = DCHECK_NOTNULL(d_);
  base::Result r;
  while (running_) {
    lock.unlock();
    auto reacquire0 = base::cleanup([&lock] { lock.lock(); });
    r = d->donate(false);
    reacquire0.run();

    if (!donate_ok(r)) break;
    if (!forever) break;
  }
  r.expect_ok(__FILE__, __LINE__);
  return base::Result();
}

void ManagerImpl::handle_event(CallbackVec* cbvec, base::token_t gt, Set set) {
  DCHECK_NOTNULL(cbvec);

  if (gt == token_t()) {
    handle_pipe_event(cbvec);
  }

  auto srcit = sources_.find(gt);
  if (srcit == sources_.end()) return;
  const auto& src = srcit->second;

  switch (src.type) {
    case SourceType::fd:
      handle_fd_event(cbvec, src, set);
      break;

    case SourceType::timer:
      handle_timer_event(cbvec, src);
      break;

    default:
      LOG(DFATAL) << "BUG: unexpected event handler type " << uint8_t(src.type);
  }
}

void ManagerImpl::handle_pipe_event(CallbackVec* cbvec) {
  DCHECK_NOTNULL(cbvec);

  Data data;
  base::Result r;
  while (true) {
    r = base::read_exactly(pipe_.read, &data, sizeof(data), "event pipe");
    if (r.code() == base::Result::Code::END_OF_FILE) return;
    if (r.errno_value() == EAGAIN || r.errno_value() == EWOULDBLOCK) return;
    r.expect_ok(__FILE__, __LINE__);
    if (!r) return;

    if (data.events.signal()) {
      auto sigit = sigmap_.find(data.signal_number);
      if (sigit == sigmap_.end()) continue;
      token_t gt = sigit->second;

      auto srcit = sources_.find(gt);
      if (srcit == sources_.end()) continue;
      if (srcit->second.type != SourceType::signal) continue;
      if (srcit->second.signo != data.signal_number) continue;
      const auto& src = srcit->second;

      for (const auto& rec : src.records) {
        data.token = rec->t;
        cbvec->push_back(handler_callback(rec.get(), data));
      }
    }

    if (data.events.event()) {
      auto srcit = sources_.find(data.token);
      if (srcit == sources_.end()) continue;
      if (srcit->second.type != SourceType::generic) continue;
      const auto& src = srcit->second;

      for (const auto& rec : src.records) {
        cbvec->push_back(handler_callback(rec.get(), data));
      }
    }
  }
}

void ManagerImpl::handle_timer_event(CallbackVec* cbvec, const Source& src) {
  static constexpr uint64_t INTMAX = std::numeric_limits<int>::max();
  DCHECK_NOTNULL(cbvec);

  uint64_t x = 0;
  auto r = base::read_exactly(src.fd, &x, sizeof(x), "timerfd");
  r.expect_ok(__FILE__, __LINE__);
  if (x > INTMAX) x = INTMAX;

  for (const auto& rec : src.records) {
    Data data;
    data.token = rec->t;
    data.int_value = x;
    data.events = Set::timer_bit();
    cbvec->push_back(handler_callback(rec.get(), data));
  }
}

void ManagerImpl::handle_fd_event(CallbackVec* cbvec, const Source& src,
                                  Set set) {
  DCHECK_NOTNULL(cbvec);

  const int fdnum = ([&src] {
    auto pair = src.fd->acquire_fd();
    return pair.first;
  })();
  for (const auto& rec : src.records) {
    auto intersection = rec->set & set;
    if (intersection) {
      Data data;
      data.token = rec->t;
      data.fd = fdnum;
      data.events = intersection;
      cbvec->push_back(handler_callback(rec.get(), data));
    }
  }
}

void ManagerImpl::wait_on_record(base::Lock lock, Record* rec) const {
  using MS = std::chrono::milliseconds;
  DispatcherPtr d = DCHECK_NOTNULL(d_);
  lock.unlock();
  auto lock1 = base::acquire_lock(rec->mu);
  while (rec->outstanding) {
    if (rec->cv.wait_for(lock1, MS(1)) == std::cv_status::timeout) {
      lock1.unlock();
      d->donate(false);
      lock1.lock();
    }
  }
}

base::Result ManagerImpl::shutdown() noexcept {
  auto lock = base::acquire_lock(mu_);

  if (!running_) return base::Result();

  // Mark ourselves as no longer running.
  running_ = false;

  // Throw away all handlers and ancilliary data.
  sources_.clear();
  ltmap_.clear();
  sigmap_.clear();
  fdmap_.clear();
  sig_tee_remove_all(pipe_.write);

  // Wait for the pollers to notice.
  while (current_ > 0) {
    std::size_t x = current_;
    event::Data data;
    base::write_exactly(pipe_.write, &data, sizeof(data), "event pipe")
        .expect_ok(__FILE__, __LINE__);
    while (current_ == x) curr_cv_.wait(lock);
  }

  // Close the event pipe write fd.
  base::Result wr = pipe_.write->close();

  // Close the event pipe read fd.
  base::Result rr = pipe_.read->close();

  // Free the dispatcher and poller.
  d_ = nullptr;
  p_ = nullptr;

  if (!wr) return wr;
  return rr;
}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept {
  release().expect_ok(__FILE__, __LINE__);
  swap(other);
  return *this;
}

void FileDescriptor::assert_valid() const {
  CHECK(ptr_) << ": event::FileDescriptor is empty!";
}

base::Result FileDescriptor::get(Set* out) const {
  assert_valid();
  DCHECK_NOTNULL(out);
  return ptr_->fd_get(out, t_);
}

base::Result FileDescriptor::modify(Set set) {
  assert_valid();
  return ptr_->fd_modify(t_, set);
}

base::Result FileDescriptor::release() {
  ManagerPtr ptr;
  ptr.swap(ptr_);
  if (ptr) return ptr->fd_remove(t_);
  return base::Result();
}

Signal& Signal::operator=(Signal&& other) noexcept {
  release().expect_ok(__FILE__, __LINE__);
  swap(other);
  return *this;
}

void Signal::assert_valid() const {
  CHECK(ptr_) << ": event::Signal is empty!";
}

base::Result Signal::release() {
  ManagerPtr ptr;
  ptr.swap(ptr_);
  if (ptr) return ptr->signal_remove(t_);
  return base::Result();
}

Timer& Timer::operator=(Timer&& other) noexcept {
  release().expect_ok(__FILE__, __LINE__);
  swap(other);
  return *this;
}

void Timer::assert_valid() const { CHECK(ptr_) << ": event::Timer is empty!"; }

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

base::Result Timer::set_periodic_at(base::Duration period,
                                    base::MonotonicTime at) {
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
  ManagerPtr ptr;
  ptr.swap(ptr_);
  if (ptr) return ptr->timer_remove(t_);
  return base::Result();
}

Generic& Generic::operator=(Generic&& other) noexcept {
  release().expect_ok(__FILE__, __LINE__);
  swap(other);
  return *this;
}

void Generic::assert_valid() const {
  CHECK(ptr_) << ": event::Generic is empty!";
}

base::Result Generic::fire(int value) const {
  assert_valid();
  return ptr_->generic_fire(t_, value);
}

base::Result Generic::release() {
  ManagerPtr ptr;
  ptr.swap(ptr_);
  if (ptr) return ptr->generic_remove(t_);
  return base::Result();
}

void Manager::assert_valid() const {
  CHECK(ptr_) << ": event::Manager is empty!";
}

PollerPtr Manager::poller() const {
  assert_valid();
  return ptr_->poller();
}

DispatcherPtr Manager::dispatcher() const {
  assert_valid();
  return ptr_->dispatcher();
}

base::Result Manager::fd(FileDescriptor* out, base::FD fd, Set set,
                         HandlerPtr handler) const {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(fd);
  DCHECK_NOTNULL(handler);
  assert_valid();
  base::Result r = out->release();
  if (r) {
    base::token_t t;
    r = ptr_->fd_add(&t, fd, set, std::move(handler));
    if (r) {
      *out = FileDescriptor(ptr_, std::move(fd), t);
    }
  }
  return r;
}

base::Result Manager::signal(Signal* out, int signo, HandlerPtr handler) const {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(handler);
  assert_valid();
  base::Result r = out->release();
  if (r) {
    base::token_t t;
    r = ptr_->signal_add(&t, signo, std::move(handler));
    if (r) {
      *out = Signal(ptr_, signo, t);
    }
  }
  return r;
}

base::Result Manager::timer(Timer* out, HandlerPtr handler) const {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(handler);
  assert_valid();
  base::Result r = out->release();
  if (r) {
    base::token_t t;
    r = ptr_->timer_add(&t, std::move(handler));
    if (r) {
      *out = Timer(ptr_, t);
    }
  }
  return r;
}

base::Result Manager::generic(Generic* out, HandlerPtr handler) const {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(handler);
  assert_valid();
  base::Result r = out->release();
  if (r) {
    base::token_t t;
    r = ptr_->generic_add(&t, std::move(handler));
    if (r) {
      *out = Generic(ptr_, t);
    }
  }
  return r;
}

base::Result Manager::set_deadline(Task* task, base::MonotonicTime at) {
  DCHECK_NOTNULL(task);
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
  DCHECK_NOTNULL(task);
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
  if (n > tv.size()) {
    LOG(DFATAL) << "BUG: event::wait_n asked to wait for " << n
                << " task completions, but only " << tv.size()
                << " tasks were provided!";
    n = tv.size();
  }

  auto closure = [](std::shared_ptr<WaitData> data) {
    VLOG(4) << "hello from event::wait_n closure";
    auto lock = base::acquire_lock(data->mu);
    ++data->done;
    data->cv.notify_all();
    return base::Result();
  };

  auto data = std::make_shared<WaitData>();
  for (Task* task : tv) {
    DCHECK_NOTNULL(task);
    task->on_finished(callback(closure, data));
  }

  bool all_threaded = true;
  for (const Manager& m : mv) {
    if (m.dispatcher()->type() != DispatcherType::threaded_dispatcher) {
      all_threaded = false;
      break;
    }
  }

  auto lock = base::acquire_lock(data->mu);
  while (data->done < n) {
    // Inline? Maybe it's blocked on I/O. Try donating.
    // Async? Just donate.
    // Threaded? Don't be so eager to join the fray.
    if (all_threaded) {
      VLOG(5) << "event::wait_n: blocking";
      data->cv.wait(lock);
    } else {
      lock.unlock();
      for (const Manager& m : mv) {
        CHECK_OK(m.donate(false));
      }
      lock.lock();
    }
  }
}

static base::Result make_manager(ManagerPtr* out, const ManagerOptions& o) {
  DCHECK_NOTNULL(out);

  std::size_t min, max;
  bool has_min, has_max;
  std::tie(has_min, min) = o.min_pollers();
  std::tie(has_max, max) = o.max_pollers();
  if (!has_min) min = 1;
  if (!has_max) max = min;
  if (min > max)
    return base::Result::invalid_argument("min_pollers > max_pollers");
  if (max < 1) return base::Result::invalid_argument("max_pollers < 1");

  base::Pipe pipe;
  base::Result r = base::make_pipe(&pipe);
  if (!r) return r;

  PollerPtr p;
  r = new_poller(&p, o.poller());
  if (!r) return r;

  r = p->add(pipe.read, token_t(), Set::readable_bit());
  if (!r) return r;

  DispatcherPtr d;
  r = new_dispatcher(&d, o.dispatcher());
  if (!r) return r;

  *out =
      std::make_shared<ManagerImpl>(std::move(p), std::move(d), pipe, min, max);
  return r;
}

base::Result new_manager(Manager* out, const ManagerOptions& o) {
  DCHECK_NOTNULL(out)->reset();
  ManagerPtr ptr;
  auto r = make_manager(&ptr, o);
  if (r) *out = Manager(std::move(ptr));
  return r;
}

static std::mutex g_sysmgr_mu;

static Manager* g_sysmgr_ptr = nullptr;

Manager system_manager() {
  auto lock = base::acquire_lock(g_sysmgr_mu);
  if (g_sysmgr_ptr == nullptr) {
    ManagerOptions o;
    std::unique_ptr<Manager> m(new Manager);
    CHECK_OK(new_manager(m.get(), o));
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
