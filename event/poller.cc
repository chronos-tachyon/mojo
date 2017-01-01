// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/poller.h"

#include <poll.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/mutex.h"

static short poll_mask(event::Set set) noexcept {
  short result = 0;
  if (set.readable()) result |= POLLIN | POLLRDHUP;
  if (set.writable()) result |= POLLOUT;
  if (set.priority()) result |= POLLPRI;
  return result;
}

static event::Set poll_unmask(short bits) noexcept {
  event::Set set;
  set.set_readable(bits & (POLLIN | POLLRDHUP));
  set.set_writable(bits & POLLOUT);
  set.set_priority(bits & POLLPRI);
  set.set_hangup(bits & POLLHUP);
  set.set_error(bits & POLLERR);
  return set;
}

static uint32_t epoll_mask(event::Set set) noexcept {
  uint32_t result = EPOLLET;
  if (set.readable()) result |= EPOLLIN | EPOLLRDHUP;
  if (set.writable()) result |= EPOLLOUT;
  if (set.priority()) result |= EPOLLPRI;
  return result;
}

static event::Set epoll_unmask(uint32_t bits) noexcept {
  event::Set set;
  set.set_readable(bits & (EPOLLIN | EPOLLRDHUP));
  set.set_writable(bits & EPOLLOUT);
  set.set_priority(bits & EPOLLPRI);
  set.set_hangup(bits & EPOLLHUP);
  set.set_error(bits & EPOLLERR);
  return set;
}

namespace event {

namespace {

struct Item {
  base::FD filedesc;
  base::token_t token;
  Set set;

  Item() noexcept = default;
  Item(base::FD fd, base::token_t t, Set s) noexcept : filedesc(std::move(fd)),
                                                       token(t),
                                                       set(s) {}
};

struct Activation {
  base::RLock lock;
  int fdnum;
  base::token_t token;
  Set set;

  Activation(base::RLock lk, int n, base::token_t t, Set s) noexcept
      : lock(std::move(lk)),
        fdnum(n),
        token(t),
        set(s) {}
};

class LegacyPoller : public Poller {
 protected:
  explicit LegacyPoller(bool maintain_nfds) : nfds_(0), maint_(maintain_nfds) {}

 public:
  base::Result add(base::FD fd, base::token_t t, Set set) override {
    auto lock = base::acquire_lock(mu_);
    auto fdpair = fd->acquire_fd();
    auto it = map_.find(fdpair.first);
    if (it != map_.end()) {
      return base::Result::from_errno(EEXIST, "net::Poller::add");
    }
    map_[fdpair.first] = Item(std::move(fd), t, set);
    if (maint_ && fdpair.first >= nfds_) nfds_ = fdpair.first + 1;
    return base::Result();
  }

  base::Result modify(base::FD fd, base::token_t t, Set set) override {
    auto lock = base::acquire_lock(mu_);
    auto fdpair = fd->acquire_fd();
    auto it = map_.find(fdpair.first);
    if (it == map_.end()) {
      return base::Result::from_errno(ENOENT, "net::Poller::modify");
    }
    auto& item = it->second;
    item.token = t;
    item.set = set;
    return base::Result();
  }

  base::Result remove(base::FD fd) override {
    auto lock = base::acquire_lock(mu_);
    auto fdpair = fd->acquire_fd();
    auto it = map_.find(fdpair.first);
    if (it == map_.end()) {
      return base::Result::from_errno(ENOENT, "net::Poller::remove");
    }
    map_.erase(it);
    if (maint_ && nfds_ == fdpair.first + 1) {
      int new_nfds = 0;
      for (const auto& pair : map_) {
        if (pair.first >= new_nfds) new_nfds = pair.first + 1;
      }
      nfds_ = new_nfds;
    }
    return base::Result();
  }

 protected:
  mutable std::mutex mu_;
  std::map<int, Item> map_;
  int nfds_;
  bool maint_;
};

class SelectPoller : public LegacyPoller {
 public:
  SelectPoller() : LegacyPoller(true) {}

  PollerType type() const noexcept override {
    return PollerType::select_poller;
  }

  base::Result wait(EventVec* out, int timeout_ms) const override {
    auto lock = base::acquire_lock(mu_);
    const std::size_t n = map_.size();

    fd_set rset, wset, eset;
    FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_ZERO(&eset);

    std::vector<Activation> acts;
    acts.reserve(n);
    for (const auto& pair : map_) {
      const auto& item = pair.second;
      int fdnum;
      base::RLock rlock;
      std::tie(fdnum, rlock) = item.filedesc->acquire_fd();
      acts.emplace_back(std::move(rlock), fdnum, item.token, item.set);
      if (fdnum < 0 || fdnum >= FD_SETSIZE) {
        return base::Result::invalid_argument("fd ", fdnum,
                                              " is out of range 0..",
                                              FD_SETSIZE - 1, " for select(2)");
      }
      if (item.set.readable()) FD_SET(fdnum, &rset);
      if (item.set.writable()) FD_SET(fdnum, &wset);
      FD_SET(fdnum, &eset);
    }

    struct timeval tv;
    struct timeval* tvptr = nullptr;
    if (timeout_ms >= 0) {
      ::bzero(&tv, sizeof(tv));
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;
      tvptr = &tv;
    }

    int rc = ::select(nfds_, &rset, &wset, &eset, tvptr);
    if (rc < 0) {
      int err_no = errno;
      if (err_no == EINTR) return base::Result();
      return base::Result::from_errno(err_no, "select(2)");
    }
    if (rc > 0) {
      for (auto& act : acts) {
        Set set;
        set.set_readable(FD_ISSET(act.fdnum, &rset));
        set.set_writable(FD_ISSET(act.fdnum, &wset));
        set.set_error(FD_ISSET(act.fdnum, &eset));
        if (set) out->emplace_back(act.token, set);
      }
    }
    return base::Result();
  }
};

class PollPoller : public LegacyPoller {
 public:
  PollPoller() : LegacyPoller(false) {}

  PollerType type() const noexcept override { return PollerType::poll_poller; }

  base::Result wait(EventVec* out, int timeout_ms) const override {
    auto lock = base::acquire_lock(mu_);
    const std::size_t n = map_.size();

    std::vector<Activation> acts;
    acts.reserve(n);
    for (const auto& pair : map_) {
      const auto& item = pair.second;
      int fdnum;
      base::RLock rlock;
      std::tie(fdnum, rlock) = item.filedesc->acquire_fd();
      acts.emplace_back(std::move(rlock), fdnum, item.token, item.set);
    }

    std::vector<struct pollfd> pfds;
    pfds.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
      auto& act = acts[i];
      auto& pfd = pfds[i];
      ::bzero(&pfd, sizeof(pfd));
      pfd.fd = act.fdnum;
      pfd.events = poll_mask(act.set);
      pfd.revents = 0;
    }

    int rc = ::poll(pfds.data(), pfds.size(), timeout_ms);
    if (rc < 0) {
      int err_no = errno;
      if (err_no == EINTR) return base::Result();
      return base::Result::from_errno(err_no, "poll(2)");
    }
    if (rc > 0) {
      for (std::size_t i = 0; i < n; ++i) {
        auto& act = acts[i];
        auto& pfd = pfds[i];
        if (pfd.revents != 0) {
          Set set = poll_unmask(pfd.revents);
          out->emplace_back(act.token, set);
        }
      }
    }
    return base::Result();
  }
};

class EPollPoller : public Poller {
 public:
  explicit EPollPoller(int epoll_fd) noexcept : epoll_fd_(epoll_fd) {}
  ~EPollPoller() noexcept override { ::close(epoll_fd_); }

  PollerType type() const noexcept override { return PollerType::epoll_poller; }

  base::Result add(base::FD fd, base::token_t t, Set set) override {
    epoll_event ev;
    ::bzero(&ev, sizeof(ev));
    ev.events = epoll_mask(set);
    ev.data.u64 = uint64_t(t);
    auto pair = DCHECK_NOTNULL(fd)->acquire_fd();
    int rc = ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, pair.first, &ev);
    if (rc != 0) {
      int err_no = errno;
      return base::Result::from_errno(err_no, "epoll_ctl(2)");
    }
    return base::Result();
  }

  base::Result modify(base::FD fd, base::token_t t, Set set) override {
    epoll_event ev;
    ::bzero(&ev, sizeof(ev));
    ev.events = epoll_mask(set);
    ev.data.u64 = uint64_t(t);
    auto pair = DCHECK_NOTNULL(fd)->acquire_fd();
    int rc = ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, pair.first, &ev);
    if (rc != 0) {
      int err_no = errno;
      return base::Result::from_errno(err_no, "epoll_ctl(2)");
    }
    return base::Result();
  }

  base::Result remove(base::FD fd) override {
    epoll_event dummy;
    auto pair = DCHECK_NOTNULL(fd)->acquire_fd();
    int rc = ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, pair.first, &dummy);
    if (rc != 0) {
      int err_no = errno;
      return base::Result::from_errno(err_no, "epoll_ctl(2)");
    }
    return base::Result();
  }

  base::Result wait(EventVec* out, int timeout_ms) const override {
    base::Result result;
    std::array<epoll_event, 8> ev = {};
    std::size_t num = ev.size();
    while (num >= ev.size()) {
      ev.fill(epoll_event());
      int n = ::epoll_wait(epoll_fd_, ev.data(), ev.size(), timeout_ms);
      if (n < 0) {
        int err_no = errno;
        if (err_no == EINTR) break;
        result = base::Result::from_errno(err_no, "epoll_wait(2)");
        break;
      }
      num = n;
      if (num > ev.size()) n = ev.size();
      for (std::size_t i = 0; i < num; ++i) {
        Set set = epoll_unmask(ev[i].events);
        auto t = base::token_t(ev[i].data.u64);
        out->emplace_back(t, set);
      }
      timeout_ms = 0;
    }
    return result;
  }

 private:
  const int epoll_fd_;
};

base::Result new_select_poller(PollerPtr* out, const PollerOptions& opts) {
  *out = std::make_shared<SelectPoller>();
  return base::Result();
}

base::Result new_poll_poller(PollerPtr* out, const PollerOptions& opts) {
  *out = std::make_shared<PollPoller>();
  return base::Result();
}

base::Result new_epoll_poller(PollerPtr* out, const PollerOptions& opts) {
  int fd = ::epoll_create1(EPOLL_CLOEXEC);
  if (fd == -1) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "epoll_create1(2)");
  }
  *out = std::make_shared<EPollPoller>(fd);
  return base::Result();
}

}  // anonymous namespace

base::Result new_poller(PollerPtr* out, const PollerOptions& opts) {
  DCHECK_NOTNULL(out)->reset();
  auto type = opts.type();
  switch (type) {
    case PollerType::select_poller:
      return new_select_poller(out, opts);

    case PollerType::poll_poller:
      return new_poll_poller(out, opts);

    case PollerType::unspecified:
    case PollerType::epoll_poller:
      return new_epoll_poller(out, opts);

    default:
      return base::Result::not_implemented();
  }
}

}  // namespace event
