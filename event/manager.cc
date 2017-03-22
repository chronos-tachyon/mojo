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

#include "base/backport.h"
#include "base/cleanup.h"
#include "base/logging.h"
#include "base/mutex.h"

using TeeVec = std::vector<base::FD>;
using TeeMap = std::unordered_map<int, TeeVec>;
using namespace base::backport;

namespace event {

namespace {

static constexpr Set kFdMust = Set::hangup_bit() | Set::error_bit();
static constexpr Set kFdCan =
    Set::readable_bit() | Set::writable_bit() | Set::priority_bit() | kFdMust;

static base::Result is_disabled() {
  return base::Result::failed_precondition("event::Handle has been disabled");
}

static base::Result not_running() {
  return base::Result::failed_precondition("event::Manager is stopped");
}

static int get_fdnum(const base::FD& fd) {
  auto pair = fd->acquire_fd();
  return pair.first;
}

template <typename T>
static bool vec_erase_all(std::vector<T>& vec, const T& item) noexcept {
  bool found = false;
  auto begin = vec.begin(), it = vec.end();
  while (it != begin) {
    --it;
    if (*it == item) {
      vec.erase(it);
      found = true;
    }
  }
  return found;
}

static char safe_back(const std::string& str) noexcept {
  if (str.empty())
    return 0;
  else
    return str.back();
}

static std::string S(std::size_t count, std::string s, std::string p = "") {
  if (count == 1) return s;
  if (p.empty()) {
    p = s;
    switch (safe_back(p)) {
      case 'a':
      case 'i':
      case 'o':
      case 'u':
        p += 'e';
        break;

      case 'y':
        p.back() = 'i';
        p += 'e';
        break;
    }
    p += 's';
  }
  return p;
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
    if (r.code() == base::ResultCode::END_OF_FILE) break;
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

struct reacquire_lock {
  base::Lock& lock;

  explicit reacquire_lock(base::Lock& lk) noexcept : lock(lk) {}
  void operator()() const noexcept { lock.lock(); }
};

struct poll_thread {
  using MI = internal::ManagerImpl;
  MI* manager;

  explicit poll_thread(MI* m) noexcept : manager(DCHECK_NOTNULL(m)) {}
  void operator()() const noexcept { manager->donate(true); }
};

struct DeadlineHelper {
  const DispatcherPtr dispatcher;
  Task* const task;
  std::mutex mu;
  Handle timer;
  bool seen;

  struct ExpireHandler : public Handler {
    DeadlineHelper* const helper;

    explicit ExpireHandler(DeadlineHelper* h) noexcept : helper(h) {}
    base::Result run(Data) const override {
      helper->expire();
      return base::Result();
    }
  };

  struct FinishCallback : public Callback {
    DeadlineHelper* const helper;

    explicit FinishCallback(DeadlineHelper* h) noexcept : helper(h) {}
    base::Result run() override {
      helper->finish();
      return base::Result();
    }
  };

  DeadlineHelper(DispatcherPtr d, Task* t) noexcept
      : dispatcher(DCHECK_NOTNULL(std::move(d))),
        task(DCHECK_NOTNULL(t)),
        seen(false) {}

  base::Result initialize(const Manager& m, base::time::MonotonicTime at) {
    base::Result r = m.timer(&timer, std::make_shared<ExpireHandler>(this));
    if (r) {
      r = timer.set_at(at);
      if (r) task->on_finished(make_unique<FinishCallback>(this));
    }
    if (!r) finish();
    return r;
  }

  base::Result initialize(const Manager& m, base::time::Duration delay) {
    base::Result r = m.timer(&timer, std::make_shared<ExpireHandler>(this));
    if (r) {
      r = timer.set_delay(delay);
      if (r) task->on_finished(make_unique<FinishCallback>(this));
    }
    if (!r) finish();
    return r;
  }

  void expire() {
    auto lock = base::acquire_lock(mu);
    if (!seen) {
      seen = true;
      timer.disable().expect_ok(__FILE__, __LINE__);
      lock.unlock();
      task->expire();
    }
  }

  void finish() {
    auto lock = base::acquire_lock(mu);
    if (!seen) {
      seen = true;
      timer.disable().expect_ok(__FILE__, __LINE__);
    }
    lock.unlock();
    dispatcher->dispose(this);
  }
};

}  // anonymous namespace

namespace internal {

void Record::wait() noexcept {
  internal::assert_depth();
  auto lock = base::acquire_lock(mu);
  CHECK(disabled) << ": must call event.disable() first!";
  auto x = outstanding;
  VLOG(6) << x << " " << S(x, "callback") << " to wait on";
  bool threaded =
      (dispatcher->type() == event::DispatcherType::threaded_dispatcher);
  std::chrono::milliseconds timeout(1);
  if (outstanding != 0 && !threaded) {
    VLOG(5) << "event::Record::wait: donating";
    lock.unlock();
    dispatcher->donate(false);
    lock.lock();
  }
  while (outstanding != 0) {
    VLOG(5) << "event::Record::wait: blocking";
    if (cv.wait_for(lock, timeout) == std::cv_status::timeout) {
      VLOG(5) << "event::Record::wait: donating";
      lock.unlock();
      dispatcher->donate(false);
      lock.lock();
      timeout *= 2;
    }
    x = outstanding;
    VLOG(6) << x << " " << S(x, "callback remains", "callbacks remain");
  }
}

HandlerCallback::~HandlerCallback() noexcept {
  auto lock = base::acquire_lock(rec->mu);
  auto x = --rec->outstanding;
  if (x == 0) rec->cv.notify_all();
  VLOG(6) << "Destroyed a callback; " << x << " more "
          << S(x, "remains", "remain");
}

base::Result HandlerCallback::run() {
  auto lock = base::acquire_lock(rec->mu);
  if (rec->disabled) return base::Result();
  auto h = rec->handler;
  lock.unlock();
  VLOG(6) << "Running a callback";
  return h->run(std::move(data));
}

ManagerImpl::ManagerImpl(PollerPtr p, DispatcherPtr d, base::Pipe pipe,
                         std::size_t num)
    : p_(DCHECK_NOTNULL(std::move(p))),
      d_(DCHECK_NOTNULL(std::move(d))),
      pipe_(std::move(pipe)),
      num_(num),
      current_(0),
      running_(true) {
  DCHECK_NOTNULL(pipe_.write);
  DCHECK_NOTNULL(pipe_.read);
  auto lock = base::acquire_lock(mu_);
  for (std::size_t i = 0; i < num_; ++i) {
    std::thread(poll_thread(this)).detach();
  }
  while (current_ < num_) curr_cv_.wait(lock);
}

PollerPtr ManagerImpl::poller() const noexcept {
  auto lock = base::acquire_lock(mu_);
  return CHECK_NOTNULL(p_);
}

DispatcherPtr ManagerImpl::dispatcher() const noexcept {
  auto lock = base::acquire_lock(mu_);
  return CHECK_NOTNULL(d_);
}

base::Result ManagerImpl::fd_add(std::unique_ptr<Record>* out, base::FD fd,
                                 Set set, HandlerPtr handler) {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(fd);
  DCHECK_NOTNULL(handler);
  out->reset();

  auto lock0 = base::acquire_lock(mu_);
  if (!running_) return not_running();
  PollerPtr p = DCHECK_NOTNULL(p_);

  const int fdnum = get_fdnum(fd);
  if (fdnum == -1)
    return base::Result::invalid_argument("file descriptor is closed");

  base::token_t t;
  bool added_fd;
  auto fdit = fdmap_.find(fdnum);
  if (fdit == fdmap_.end()) {
    t = base::next_token();
    fdmap_[fdnum] = t;
    added_fd = true;
  } else {
    t = fdit->second;
    added_fd = false;
  }
  auto cleanup0 = base::cleanup([this, fdnum, added_fd] {
    if (added_fd) fdmap_.erase(fdnum);
  });

  Set before;
  bool added_src;
  auto& src = sources_[t];
  if (src.records.empty()) {
    src.type = SourceType::fd;
    src.fd = std::move(fd);
    src.signo = fdnum;  // for fdmap_.erase() and handle_fd_event
    added_src = true;
  } else {
    DCHECK_EQ(src.signo, fdnum);
    CHECK_EQ(src.fd, fd);
    for (const Record* rec : src.records) {
      auto lock1 = base::acquire_lock(rec->mu);
      before |= rec->set;
    }
    added_src = false;
    fd = nullptr;
  }
  DCHECK_EQ(added_fd, added_src);

  set &= kFdCan;
  set |= kFdMust;

  Set after = before | set;
  auto myrec = make_unique<Record>(t, d_, std::move(handler), set);
  src.records.push_back(myrec.get());
  auto cleanup1 = base::cleanup([this, t, added_src, &src] {
    src.records.pop_back();
    if (added_src) sources_.erase(t);
  });

  base::Result r;
  if (added_src) {
    r = p->add(src.fd, t, after);
  } else {
    if (before != after) {
      r = p->modify(src.fd, t, after);
    }
  }
  if (r) {
    *out = std::move(myrec);
    cleanup1.cancel();
    cleanup0.cancel();
  }
  return r;
}

base::Result ManagerImpl::signal_add(std::unique_ptr<Record>* out, int signo,
                                     HandlerPtr handler) {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(handler);
  out->reset();

  if (signo < 0 || std::size_t(signo) >= NUM_SIGNALS) {
    return base::Result::invalid_argument("invalid signal number ", signo);
  }

  auto lock = base::acquire_lock(mu_);
  if (!running_) return not_running();

  base::token_t t;
  bool added_sig;
  auto sigit = sigmap_.find(signo);
  if (sigit == sigmap_.end()) {
    t = base::next_token();
    sigmap_[signo] = t;
    added_sig = true;
  } else {
    t = sigit->second;
    added_sig = false;
  }
  auto cleanup0 = base::cleanup([this, signo, added_sig] {
    if (added_sig) sigmap_.erase(signo);
  });

  bool added_src;
  auto& src = sources_[t];
  if (src.records.empty()) {
    src.type = SourceType::signal;
    src.signo = signo;
    added_src = true;
  } else {
    DCHECK_EQ(src.signo, signo);
    added_src = false;
  }
  DCHECK_EQ(added_sig, added_src);

  auto myrec =
      make_unique<Record>(t, d_, std::move(handler), Set::signal_bit());
  src.records.push_back(myrec.get());
  auto cleanup1 = base::cleanup([this, t, added_src, &src] {
    src.records.pop_back();
    if (added_src) sources_.erase(t);
  });

  base::Result r;
  if (added_src) {
    r = sig_tee_add(pipe_.write, signo);
  }
  if (r) {
    *out = std::move(myrec);
    cleanup1.cancel();
    cleanup0.cancel();
  }
  return r;
}

base::Result ManagerImpl::timer_add(std::unique_ptr<Record>* out,
                                    HandlerPtr handler) {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(handler);
  out->reset();

  int fdnum = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
  if (fdnum == -1) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "timerfd_create(2)");
  }
  base::FD fd = base::wrapfd(fdnum);

  auto lock = base::acquire_lock(mu_);
  if (!running_) return not_running();
  PollerPtr p = DCHECK_NOTNULL(p_);

  base::token_t t = base::next_token();
  auto& src = sources_[t];
  src.type = SourceType::timer;
  src.fd = std::move(fd);

  auto myrec = make_unique<Record>(t, d_, std::move(handler), Set::timer_bit());
  src.records.push_back(myrec.get());
  auto cleanup = base::cleanup([this, t] { sources_.erase(t); });

  base::Result r = p->add(src.fd, t, Set::readable_bit());
  if (r) {
    *out = std::move(myrec);
    cleanup.cancel();
  }
  return r;
}

base::Result ManagerImpl::generic_add(std::unique_ptr<Record>* out,
                                      HandlerPtr handler) {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(handler);
  out->reset();

  auto lock = base::acquire_lock(mu_);
  if (!running_) return not_running();

  base::token_t t = base::next_token();
  auto& src = sources_[t];
  src.type = SourceType::generic;

  auto myrec =
      make_unique<Record>(t, d_, std::move(handler), Set::generic_bit());
  src.records.push_back(myrec.get());
  *out = std::move(myrec);
  return base::Result();
}

base::Result ManagerImpl::modify(Record* myrec, Set set) {
  CHECK_NOTNULL(myrec);

  auto lock0 = base::acquire_lock(mu_);
  auto lock1 = base::acquire_lock(myrec->mu);
  if (myrec->disabled) return is_disabled();
  if (!running_) return not_running();
  PollerPtr p = DCHECK_NOTNULL(p_);

  auto t = myrec->token;
  auto srcit = sources_.find(t);
  DCHECK(srcit != sources_.end());
  auto& src = srcit->second;

  if (src.type != SourceType::fd) {
    return base::Result::wrong_type("event::Handle: not an FD");
  }

  set &= kFdCan;
  set |= kFdMust;

  Set before, after;
  bool found = false;
  for (const Record* rec : src.records) {
    if (rec == myrec) {
      before |= myrec->set;
      after |= set;
      found = true;
    } else {
      auto lock2 = base::acquire_lock(rec->mu);
      before |= rec->set;
      after |= rec->set;
    }
  }
  DCHECK(found);

  base::Result r;
  if (before != after) {
    r = p->modify(src.fd, t, after);
  }
  if (r) myrec->set = set;
  return r;
}

base::Result ManagerImpl::arm(Record* myrec, base::time::Duration delay,
                              base::time::Duration period, bool delay_abs) {
  DCHECK_NOTNULL(myrec);

  auto lock0 = base::acquire_lock(mu_);
  auto lock1 = base::acquire_lock(myrec->mu);
  if (myrec->disabled) return is_disabled();
  if (!running_) return not_running();

  auto srcit = sources_.find(myrec->token);
  DCHECK(srcit != sources_.end());
  auto& src = srcit->second;

  if (src.type != SourceType::timer) {
    return base::Result::wrong_type("event::Handle: not a timer");
  }

  int flags = 0;
  if (delay_abs) flags |= TFD_TIMER_ABSTIME;

  struct itimerspec its;
  ::bzero(&its, sizeof(its));
  auto r0 = base::time::timespec_from_duration(&its.it_value, delay);
  auto r1 = base::time::timespec_from_duration(&its.it_interval, period);
  auto r = r0.and_then(r1);
  if (!r) return r;

  auto pair = src.fd->acquire_fd();
  int rc = ::timerfd_settime(pair.first, flags, &its, nullptr);
  int err_no = errno;
  pair.second.unlock();

  if (rc != 0) {
    return base::Result::from_errno(err_no, "timerfd_settime(2)");
  }
  return base::Result();
}

base::Result ManagerImpl::fire(Record* myrec, int value) {
  DCHECK_NOTNULL(myrec);

  auto lock0 = base::acquire_lock(mu_);
  auto lock1 = base::acquire_lock(myrec->mu);
  if (myrec->disabled) is_disabled();
  if (!running_) return not_running();

  auto srcit = sources_.find(myrec->token);
  DCHECK(srcit != sources_.end());
  auto& src = srcit->second;

  if (src.type != SourceType::generic) {
    return base::Result::wrong_type("event::Handle: not a generic");
  }

  Data data;
  data.token = myrec->token;
  data.int_value = value;
  data.events = Set::generic_bit();
  return base::write_exactly(pipe_.write, &data, sizeof(data), "event pipe");
}

base::Result ManagerImpl::disable(Record* myrec) {
  DCHECK_NOTNULL(myrec);

  auto lock0 = base::acquire_lock(mu_);
  auto lock1 = base::acquire_lock(myrec->mu);
  if (myrec->disabled) return base::Result();
  if (!running_) {
    myrec->disabled = true;
    return base::Result();
  }
  PollerPtr p = DCHECK_NOTNULL(p_);

  auto t = myrec->token;
  auto srcit = sources_.find(t);
  DCHECK(srcit != sources_.end());
  auto& src = srcit->second;

  Set before, after;
  bool found = false;
  auto rit = src.records.end(), rbegin = src.records.begin();
  while (rit != rbegin) {
    --rit;
    Record* rec = *rit;
    if (rec == myrec) {
      before |= myrec->set;
      found = true;
      src.records.erase(rit);
    } else {
      auto lock2 = base::acquire_lock(rec->mu);
      before |= rec->set;
      after |= rec->set;
    }
  }
  DCHECK(found);

  base::Result r;
  if (src.records.empty()) {
    base::FD fd;
    int fdnum, signo;
    switch (src.type) {
      case SourceType::undefined:
        LOG(DFATAL) << "BUG! Attempt to disable an undefined event type";
        break;

      case SourceType::fd:
        fd = std::move(src.fd);
        fdnum = src.signo;
        sources_.erase(t);
        fdmap_.erase(fdnum);
        r = p->remove(fd);
        break;

      case SourceType::signal:
        signo = src.signo;
        sigmap_.erase(signo);
        sources_.erase(t);
        r = sig_tee_remove(pipe_.write, signo);
        break;

      case SourceType::timer:
        fd = std::move(src.fd);
        sources_.erase(srcit);
        r = p->remove(fd);
        r = r.and_then(fd->close());
        break;

      case SourceType::generic:
        sources_.erase(srcit);
        break;
    }
  } else if (src.type == SourceType::fd && before != after) {
    r = p->modify(src.fd, t, after);
  }

  myrec->disabled = true;
  return r;
}

void ManagerImpl::donate(bool forever) noexcept {
  auto lock = base::acquire_lock(mu_);
  if (forever)
    donate_forever(lock);
  else
    donate_once(lock);
}

void ManagerImpl::shutdown() noexcept {
  auto lock0 = base::acquire_lock(mu_);
  if (!running_) return;

  // Mark ourselves as no longer running.
  running_ = false;

  VLOG(6) << "Collecting records";
  std::vector<Record*> records;
  for (auto& pair : sources_) {
    auto& v = pair.second.records;
    records.insert(records.end(), v.begin(), v.end());
  }

  VLOG(6) << "Clearing ancillary data";
  sources_.clear();
  sigmap_.clear();
  fdmap_.clear();
  sig_tee_remove_all(pipe_.write);

  // Wait for the poller threads to notice.
  while (current_ > 0) {
    auto x = current_;
    VLOG(6) << "Stopping " << x << " poller " << S(x, "thread");
    event::Data data;
    base::write_exactly(pipe_.write, &data, sizeof(data), "event pipe")
        .expect_ok(__FILE__, __LINE__);
    while (current_ == x) curr_cv_.wait(lock0);
  }

  VLOG(6) << "Closing event pipe (write half)";
  pipe_.write->close().expect_ok(__FILE__, __LINE__);

  VLOG(6) << "Closing event pipe (read half)";
  pipe_.read->close().expect_ok(__FILE__, __LINE__);

  VLOG(6) << "Freeing poller";
  p_ = nullptr;

  auto x = records.size();
  VLOG(6) << "Marking " << x << " " << S(x, "record") << " as disabled";
  for (Record* rec : records) {
    auto lock1 = base::acquire_lock(rec->mu);
    rec->disabled = true;
  }

  VLOG(6) << "Waiting on " << x << " " << S(x, "record");
  for (Record* rec : records) {
    rec->wait();
  }

  VLOG(6) << "Freeing dispatcher";
  d_ = nullptr;
}

void ManagerImpl::inc_current(base::Lock& lock) noexcept {
  ++current_;
  curr_cv_.notify_all();
}

void ManagerImpl::dec_current(base::Lock& lock) noexcept {
  --current_;
  curr_cv_.notify_all();
}

void ManagerImpl::donate_once(base::Lock& lock) noexcept {
  DispatcherPtr d = DCHECK_NOTNULL(d_);
  PollerPtr p = DCHECK_NOTNULL(p_);

  Poller::EventVec vec;
  p->wait(&vec, 0).expect_ok(__FILE__, __LINE__);

  CallbackVec cbvec;
  for (const auto& ev : vec) {
    handle_event(&cbvec, ev.first, ev.second);
  }

  lock.unlock();
  auto reacquire = base::cleanup(reacquire_lock(lock));
  for (auto& cb : cbvec) {
    d->dispatch(nullptr, std::move(cb));
  }
  d->donate(false);
}

void ManagerImpl::donate_forever(base::Lock& lock) noexcept {
  DispatcherPtr d = DCHECK_NOTNULL(d_);
  PollerPtr p = DCHECK_NOTNULL(p_);

  inc_current(lock);
  auto cleanup = base::cleanup([this, &lock] { dec_current(lock); });

  Poller::EventVec vec;
  CallbackVec cbvec;
  while (running_) {
    lock.unlock();
    auto reacquire0 = base::cleanup(reacquire_lock(lock));
    p->wait(&vec, -1).expect_ok(__FILE__, __LINE__);
    reacquire0.run();

    for (const auto& ev : vec) {
      handle_event(&cbvec, ev.first, ev.second);
    }
    vec.clear();

    lock.unlock();
    auto reacquire1 = base::cleanup(reacquire_lock(lock));
    for (auto& cb : cbvec) {
      d->dispatch(nullptr, std::move(cb));
    }
    cbvec.clear();
    reacquire1.run();
  }
}

void ManagerImpl::schedule(CallbackVec* cbvec, Record* rec, Set set,
                           Data data) {
  DCHECK_NOTNULL(rec);
  auto lock = base::acquire_lock(rec->mu);
  if (rec->disabled) return;
  auto intersection = rec->set & set;
  if (!intersection) return;
  data.events = set;
  cbvec->push_back(make_unique<HandlerCallback>(rec, std::move(data)));
  auto x = rec->outstanding;
  VLOG(6) << "Scheduled a callback; now " << x << " " << S(x, "is", "are")
          << " outstanding";
}

void ManagerImpl::handle_event(CallbackVec* cbvec, base::token_t t, Set set) {
  DCHECK_NOTNULL(cbvec);

  if (t == base::token_t()) {
    handle_pipe_event(cbvec);
  }

  auto srcit = sources_.find(t);
  if (srcit == sources_.end()) return;
  const auto& src = srcit->second;

  switch (src.type) {
    case SourceType::fd:
      handle_fd_event(cbvec, t, src, set);
      break;

    case SourceType::timer:
      handle_timer_event(cbvec, t, src);
      break;

    default:
      LOG(DFATAL) << "BUG: unexpected event handler type "
                  << uint16_t(src.type);
  }
}

void ManagerImpl::handle_pipe_event(CallbackVec* cbvec) {
  DCHECK_NOTNULL(cbvec);

  Data data;
  base::Result r;
  while (true) {
    r = base::read_exactly(pipe_.read, &data, sizeof(data), "event pipe");
    if (r.code() == base::ResultCode::END_OF_FILE) return;
    if (r.errno_value() == EAGAIN || r.errno_value() == EWOULDBLOCK) return;
    r.expect_ok(__FILE__, __LINE__);
    if (!r) return;

    if (data.events.signal()) {
      auto sigit = sigmap_.find(data.signal_number);
      if (sigit == sigmap_.end()) continue;
      base::token_t t = sigit->second;

      auto srcit = sources_.find(t);
      if (srcit == sources_.end()) continue;
      if (srcit->second.type != SourceType::signal) continue;
      if (srcit->second.signo != data.signal_number) continue;
      const auto& src = srcit->second;

      data.token = t;
      for (Record* rec : src.records) {
        schedule(cbvec, rec, Set::signal_bit(), data);
      }
    }

    if (data.events.generic()) {
      auto srcit = sources_.find(data.token);
      if (srcit == sources_.end()) continue;
      if (srcit->second.type != SourceType::generic) continue;
      const auto& src = srcit->second;

      for (Record* rec : src.records) {
        schedule(cbvec, rec, Set::generic_bit(), data);
      }
    }
  }
}

void ManagerImpl::handle_fd_event(CallbackVec* cbvec, base::token_t t,
                                  const Source& src, Set set) {
  DCHECK_NOTNULL(cbvec);

  int fdnum = src.signo;
  Data data;
  data.token = t;
  data.fd = fdnum;
  for (Record* rec : src.records) {
    schedule(cbvec, rec, set, data);
  }
}

void ManagerImpl::handle_timer_event(CallbackVec* cbvec, base::token_t t,
                                     const Source& src) {
  static constexpr uint64_t INTMAX = std::numeric_limits<int>::max();
  DCHECK_NOTNULL(cbvec);

  uint64_t x = 0;
  auto r = base::read_exactly(src.fd, &x, sizeof(x), "timerfd");
  r.expect_ok(__FILE__, __LINE__);
  if (x > INTMAX) x = INTMAX;

  Data data;
  data.token = t;
  data.int_value = x;
  for (Record* rec : src.records) {
    schedule(cbvec, rec, Set::timer_bit(), data);
  }
}

}  // namespace internal

static void wait_impl(ManagerPtr& ptr, RecordPtr& rec) {
  auto p = std::move(ptr);
  auto r = std::move(rec);
  if (r) r->wait();
}

static void disown_impl(ManagerPtr& ptr, RecordPtr& rec) {
  auto p = std::move(ptr);
  auto r = std::move(rec);
  if (r) {
    internal::Record* record = r.release();
    record->dispatcher->dispose(record);
  }
}

void Handle::assert_valid() const {
  if (rec_ && ptr_) return;
  if (rec_) LOG(FATAL) << "BUG! rec_ != nullptr BUT ptr_ == nullptr";
  if (ptr_) LOG(FATAL) << "BUG! ptr_ != nullptr BUT rec_ == nullptr";
  LOG(FATAL) << "BUG! event::Handle is empty!";
}

base::Result Handle::get(Set* out) const {
  DCHECK_NOTNULL(out);
  assert_valid();
  auto lock = base::acquire_lock(rec_->mu);
  *out = rec_->set;
  return base::Result();
}

base::Result Handle::modify(Set set) const {
  assert_valid();
  return ptr_->modify(rec_.get(), set);
}

base::Result Handle::set_at(base::time::MonotonicTime at) const {
  assert_valid();
  base::time::Duration delay = at.since_epoch();
  if (delay.is_zero() || delay.is_neg())
    return base::Result::invalid_argument(
        "initial event must be strictly after the epoch");
  return ptr_->arm(rec_.get(), delay, base::time::Duration(), true);
}

base::Result Handle::set_delay(base::time::Duration delay) const {
  assert_valid();
  if (delay.is_zero() || delay.is_neg())
    return base::Result::invalid_argument(
        "delay must be strictly after the present");
  return ptr_->arm(rec_.get(), delay, base::time::Duration(), false);
}

base::Result Handle::set_periodic(base::time::Duration period) const {
  assert_valid();
  if (period.is_zero() || period.is_neg())
    return base::Result::invalid_argument(
        "zero or negative period doesn't make sense");
  return ptr_->arm(rec_.get(), period, period, false);
}

base::Result Handle::set_periodic_at(base::time::Duration period,
                                     base::time::MonotonicTime at) const {
  assert_valid();
  base::time::Duration delay = at.since_epoch();
  if (period.is_zero() || period.is_neg())
    return base::Result::invalid_argument(
        "zero or negative period doesn't make sense");
  if (delay.is_zero() || delay.is_neg())
    return base::Result::invalid_argument(
        "initial event must be strictly after the epoch");
  return ptr_->arm(rec_.get(), delay, period, true);
}

base::Result Handle::set_periodic_delay(base::time::Duration period,
                                        base::time::Duration delay) const {
  assert_valid();
  if (period.is_zero() || period.is_neg())
    return base::Result::invalid_argument(
        "zero or negative period doesn't make sense");
  if (delay.is_zero() || delay.is_neg())
    return base::Result::invalid_argument(
        "delay must be strictly after the present");
  return ptr_->arm(rec_.get(), delay, period, false);
}

base::Result Handle::cancel() const {
  assert_valid();
  base::time::Duration zero;
  return ptr_->arm(rec_.get(), zero, zero, false);
}

base::Result Handle::fire(int value) const {
  assert_valid();
  return ptr_->fire(rec_.get(), value);
}

base::Result Handle::disable() const {
  if (ptr_ && rec_) return ptr_->disable(rec_.get());
  return base::Result();
}

void Handle::wait() { wait_impl(ptr_, rec_); }

void Handle::disown() { disown_impl(ptr_, rec_); }

base::Result Handle::release() {
  base::Result r = disable();
  wait();
  return r;
}

void Manager::assert_valid() const noexcept {
  CHECK(ptr_) << ": event::Manager is empty!";
}

base::Result Manager::fd(Handle* out, base::FD fd, Set set,
                         HandlerPtr handler) const {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(fd);
  DCHECK_NOTNULL(handler);
  assert_valid();
  RecordPtr myrec;
  base::Result r = ptr_->fd_add(&myrec, std::move(fd), set, std::move(handler));
  if (r) {
    *out = Handle(ptr_, std::move(myrec));
  }
  return r;
}

base::Result Manager::signal(Handle* out, int signo, HandlerPtr handler) const {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(handler);
  assert_valid();
  RecordPtr myrec;
  base::Result r = ptr_->signal_add(&myrec, signo, std::move(handler));
  if (r) {
    *out = Handle(ptr_, std::move(myrec));
  }
  return r;
}

base::Result Manager::timer(Handle* out, HandlerPtr handler) const {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(handler);
  assert_valid();
  RecordPtr myrec;
  base::Result r = ptr_->timer_add(&myrec, std::move(handler));
  if (r) {
    *out = Handle(ptr_, std::move(myrec));
  }
  return r;
}

base::Result Manager::generic(Handle* out, HandlerPtr handler) const {
  DCHECK_NOTNULL(out);
  DCHECK_NOTNULL(handler);
  assert_valid();
  RecordPtr myrec;
  base::Result r = ptr_->generic_add(&myrec, std::move(handler));
  if (r) {
    *out = Handle(ptr_, std::move(myrec));
  }
  return r;
}

base::Result Manager::set_deadline(Task* task, base::time::MonotonicTime at) {
  CHECK_NOTNULL(task);
  auto* helper = new DeadlineHelper(dispatcher(), task);
  return helper->initialize(*this, at);
}

base::Result Manager::set_timeout(Task* task, base::time::Duration delay) {
  CHECK_NOTNULL(task);
  auto* helper = new DeadlineHelper(dispatcher(), task);
  return helper->initialize(*this, delay);
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
  internal::assert_depth();
  auto tn = tv.size();
  CHECK_LE(n, tn);
  n = std::min(n, tn);

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
  std::chrono::milliseconds timeout(1);
  if (data->done < n && !all_threaded) {
    VLOG(5) << "event::wait_n: donating";
    lock.unlock();
    for (const Manager& m : mv) {
      m.donate(false);
    }
    lock.lock();
  }
  while (data->done < n) {
    VLOG(5) << "event::wait_n: blocking";
    if (data->cv.wait_for(lock, timeout) == std::cv_status::timeout) {
      VLOG(5) << "event::wait_n: donating";
      lock.unlock();
      for (const Manager& m : mv) {
        m.donate(false);
      }
      lock.lock();
      timeout *= 2;
    }
  }
}

static base::Result make_manager(ManagerPtr* out, const ManagerOptions& o) {
  DCHECK_NOTNULL(out);

  std::size_t num;
  bool has_num;
  std::tie(has_num, num) = o.num_pollers();
  if (!has_num) num = 1;

  base::Pipe pipe;
  base::Result r = base::make_pipe(&pipe);
  if (!r) return r;

  PollerPtr p;
  r = new_poller(&p, o.poller());
  if (!r) return r;

  r = p->add(pipe.read, base::token_t(), Set::readable_bit());
  if (!r) return r;

  DispatcherPtr d;
  r = new_dispatcher(&d, o.dispatcher());
  if (!r) return r;

  *out = std::make_shared<internal::ManagerImpl>(std::move(p), std::move(d),
                                                 pipe, num);
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
    auto m = make_unique<Manager>();
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
