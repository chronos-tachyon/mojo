// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "event/poller.h"

#include <sys/epoll.h>
#include <unistd.h>

#include "base/util.h"

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
    auto pair = DASSERT_NOTNULL(fd)->acquire_fd();
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
    auto pair = DASSERT_NOTNULL(fd)->acquire_fd();
    int rc = ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, pair.first, &ev);
    if (rc != 0) {
      int err_no = errno;
      return base::Result::from_errno(err_no, "epoll_ctl(2)");
    }
    return base::Result();
  }

  base::Result remove(base::FD fd) override {
    epoll_event dummy;
    auto pair = DASSERT_NOTNULL(fd)->acquire_fd();
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

base::Result new_epoll_poller(std::shared_ptr<Poller>* out,
                              const PollerOptions& opts) {
  int fd = ::epoll_create1(EPOLL_CLOEXEC);
  if (fd == -1) {
    int err_no = errno;
    return base::Result::from_errno(err_no, "epoll_create1(2)");
  }
  *out = std::make_shared<EPollPoller>(fd);
  return base::Result();
}

}  // anonymous namespace

base::Result new_poller(std::shared_ptr<Poller>* out,
                        const PollerOptions& opts) {
  DASSERT_NOTNULL(out)->reset();
  auto type = opts.type();
  switch (type) {
    case PollerType::unspecified:
    case PollerType::epoll_poller:
      return new_epoll_poller(out, opts);

    default:
      return base::Result::not_implemented();
  }
}

}  // namespace event
