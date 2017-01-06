// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/connfd.h"

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <exception>
#include <mutex>

#include "base/cleanup.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "io/reader.h"
#include "io/writer.h"

namespace net {

namespace {
static sockaddr* RISA(void* ptr) { return reinterpret_cast<sockaddr*>(ptr); }

static const sockaddr* RICSA(const void* ptr) {
  return reinterpret_cast<const sockaddr*>(ptr);
}

class FDConnReader : public io::ReaderImpl {
 public:
  explicit FDConnReader(base::FD fd) : r_(io::fdreader(std::move(fd))) {}

  std::size_t ideal_block_size() const noexcept override {
    return r_.ideal_block_size();
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max, const base::Options& opts) override {
    r_.read(task, out, n, min, max, opts);
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const io::Writer& w, const base::Options& opts) override {
    r_.write_to(task, n, max, w, opts);
  }

  void close(event::Task* task, const base::Options& opts) override {
    if (!prologue(task)) return;
    auto fd = internal_readerfd();
    task->finish(base::shutdown(fd, SHUT_RD));
  }

  base::FD internal_readerfd() const override {
    return r_.implementation()->internal_readerfd();
  }

 private:
  io::Reader r_;
};

class FDConnWriter : public io::WriterImpl {
 public:
  explicit FDConnWriter(base::FD fd) : w_(io::fdwriter(std::move(fd))) {}

  std::size_t ideal_block_size() const noexcept override {
    return w_.ideal_block_size();
  }

  void write(event::Task* task, std::size_t* n, const char* ptr,
             std::size_t len, const base::Options& opts) override {
    w_.write(task, n, ptr, len, opts);
  }

  void read_from(event::Task* task, std::size_t* n, std::size_t max,
                 const io::Reader& r, const base::Options& opts) override {
    w_.read_from(task, n, max, r, opts);
  }

  void close(event::Task* task, const base::Options& opts) override {
    if (!prologue(task)) return;
    auto fd = internal_writerfd();
    task->finish(base::shutdown(fd, SHUT_WR));
  }

  base::FD internal_writerfd() const override {
    return w_.implementation()->internal_writerfd();
  }

 private:
  io::Writer w_;
};

class FDConn : public ConnImpl {
 public:
  FDConn(Addr la, Addr ra, io::Reader r, io::Writer w) noexcept
      : la_(std::move(la)),
        ra_(std::move(ra)),
        r_(std::move(r)),
        w_(std::move(w)) {
    CHECK(la_);
    CHECK(ra_);
    CHECK(r_);
    CHECK(w_);
    VLOG(6) << "net::FDConn::FDConn";
  }

  ~FDConn() noexcept { VLOG(6) << "net::FDConn::~FDConn"; }

  Addr local_addr() const override { return la_; }
  Addr remote_addr() const override { return ra_; }
  io::Reader reader() override { return r_; }
  io::Writer writer() override { return w_; }

  void close(event::Task* task, const base::Options& opts) override {
    VLOG(6) << "net::FDConn::close";
    base::Result r = fd()->close();
    if (task->start()) task->finish(std::move(r));
  }

  void get_option(event::Task* task, SockOpt opt, void* optval,
                  unsigned int* optlen,
                  const base::Options& opts) const override {
    if (!task->start()) return;
    task->finish(opt.get(fd(), optval, optlen));
  }

  void set_option(event::Task* task, SockOpt opt, const void* optval,
                  unsigned int optlen, const base::Options& opts) override {
    if (!task->start()) return;
    task->finish(opt.set(fd(), optval, optlen));
  }

 private:
  base::FD fd() const noexcept {
    return w_.implementation()->internal_writerfd();
  }

  Addr la_;
  Addr ra_;
  io::Reader r_;
  io::Writer w_;
};

class FDListenConn : public ListenConnImpl {
 public:
  FDListenConn(event::Manager m, std::shared_ptr<Protocol> pr, Addr aa,
               base::FD fd, AcceptFn fn) noexcept : m_(std::move(m)),
                                                    pr_(std::move(pr)),
                                                    aa_(std::move(aa)),
                                                    fd_(std::move(fd)),
                                                    fn_(std::move(fn)),
                                                    accepting_(false) {
    auto pair = fd_->acquire_fd();
    VLOG(6) << "net::FDListenConn::FDListenConn: fd=" << pair.first << ", "
            << "bind=" << aa_;
  }

  ~FDListenConn() noexcept { VLOG(6) << "net::FDListenConn::~FDListenConn"; }

  base::Result initialize() {
    auto closure = [this](event::Data data) { return handle(data); };
    return m_.fd(&evt_, fd_, event::Set::no_bits(), event::handler(closure));
  }

  Addr listen_addr() const override { return aa_; }
  void start(event::Task* task, const base::Options& opts) override;
  void stop(event::Task* task, const base::Options& opts) override;
  void close(event::Task* task, const base::Options& opts) override;
  void get_option(event::Task* task, SockOpt opt, void* optval,
                  unsigned int* optlen,
                  const base::Options& opts) const override;
  void set_option(event::Task* task, SockOpt opt, const void* optval,
                  unsigned int optlen, const base::Options& opts) override;

 private:
  base::Result handle(event::Data) const;

  const event::Manager m_;
  const std::shared_ptr<Protocol> pr_;
  const Addr aa_;
  const base::FD fd_;
  const AcceptFn fn_;
  mutable std::mutex mu_;
  event::FileDescriptor evt_;
  bool accepting_;
};

void FDListenConn::start(event::Task* task, const base::Options& opts) {
  VLOG(6) << "net::FDListenConn::start";
  auto lock = base::acquire_lock(mu_);
  accepting_ = true;
  base::Result r = evt_.modify(event::Set::readable_bit());
  lock.unlock();
  handle(event::Data()).ignore_ok();
  if (task->start()) task->finish(std::move(r));
}

void FDListenConn::stop(event::Task* task, const base::Options& opts) {
  VLOG(6) << "net::FDListenConn::stop";
  auto lock = base::acquire_lock(mu_);
  accepting_ = false;
  base::Result r = evt_.modify(event::Set::no_bits());
  lock.unlock();
  if (task->start()) task->finish(std::move(r));
}

void FDListenConn::close(event::Task* task, const base::Options& opts) {
  VLOG(6) << "net::FDListenConn::close";
  auto lock = base::acquire_lock(mu_);
  accepting_ = false;
  base::Result r0 = evt_.disable();
  base::Result r1 = fd_->close();
  lock.unlock();
  if (task->start()) task->finish(r0.and_then(r1));
}

void FDListenConn::get_option(event::Task* task, SockOpt opt, void* optval,
                              unsigned int* optlen,
                              const base::Options& opts) const {
  if (!task->start()) return;
  task->finish(opt.get(fd_, optval, optlen));
}

void FDListenConn::set_option(event::Task* task, SockOpt opt,
                              const void* optval, unsigned int optlen,
                              const base::Options& opts) {
  if (!task->start()) return;
  task->finish(opt.set(fd_, optval, optlen));
}

base::Result FDListenConn::handle(event::Data data) const {
  VLOG(4) << "net::FDListenConn: woke, set=" << data.events;
  struct sockaddr_storage ss;
  socklen_t sslen;
  const int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;

  auto lock = base::acquire_lock(mu_);
  while (accepting_) {
    auto pair = fd_->acquire_fd();
    ::bzero(&ss, sizeof(ss));
    sslen = sizeof(ss);
    int fdnum = ::accept4(pair.first, RISA(&ss), &sslen, flags);
    if (fdnum == -1) {
      int err_no = errno;
      if (err_no == EINTR) continue;
      if (err_no == EAGAIN || err_no == EWOULDBLOCK) break;
      base::Result::from_errno(err_no, "accept4(2)")
          .expect_ok(__FILE__, __LINE__);
      break;
    }
    base::FD fd = base::wrapfd(fdnum);

    ProtocolType p = aa_.protocol_type();

    Addr ra;
    base::Result r = pr_->interpret(&ra, p, RICSA(&ss), sslen);
    r.expect_ok(__FILE__, __LINE__);
    if (!r) continue;

    ::bzero(&ss, sizeof(ss));
    sslen = sizeof(ss);
    int rc = ::getsockname(fdnum, RISA(&ss), &sslen);
    if (rc != 0) {
      int err_no = errno;
      base::Result::from_errno(err_no, "getsockname(2)")
          .expect_ok(__FILE__, __LINE__);
      continue;
    }

    Addr la;
    r = pr_->interpret(&la, p, RICSA(&ss), sslen);
    r.expect_ok(__FILE__, __LINE__);
    if (!r) continue;

    VLOG(6) << "net::FDListenConn: accept, "
            << "fdnum=" << fdnum << ", "
            << "self=" << la << ", "
            << "peer=" << ra;

    Conn conn;
    r = fdconn(&conn, std::move(la), std::move(ra), std::move(fd));
    r.expect_ok(__FILE__, __LINE__);
    if (!r) continue;

    pair.second.unlock();
    lock.unlock();
    auto reacquire = base::cleanup([&lock] { lock.lock(); });

    try {
      fn_(std::move(conn));
    } catch (...) {
      LOG_EXCEPTION(std::current_exception());
    }
  }
  return base::Result();
}

struct DialHelper {
  Protocol* const protocol;
  event::Task* const task;
  Conn* const out;
  const ProtocolType type;
  const base::FD filedesc;
  std::mutex mu;
  event::FileDescriptor evt;
  bool seen;

  DialHelper(Protocol* pr, event::Task* t, Conn* o, ProtocolType p,
             base::FD fd) noexcept : protocol(pr),
                                     task(t),
                                     out(o),
                                     type(p),
                                     filedesc(std::move(fd)),
                                     seen(false) {}

  base::Result handle(event::Data data) {
    base::Result r;

    auto lock = base::acquire_lock(mu);
    if (seen) return base::Result();
    seen = true;
    evt.disable().expect_ok(__FILE__, __LINE__);
    evt.disown();
    lock.unlock();

    sockaddr_storage ss;
    socklen_t sslen;
    Addr la, ra;
    int x, rc;
    socklen_t xlen;

    auto fdpair = filedesc->acquire_fd();

    x = 0;
    xlen = sizeof(x);
    rc = ::getsockopt(fdpair.first, SOL_SOCKET, SO_ERROR, &x, &xlen);
    if (rc != 0) {
      int err_no = errno;
      r = base::Result::from_errno(err_no, "getsockopt(2)");
      goto end;
    }
    CHECK_EQ(xlen, sizeof(x));
    if (x != 0) {
      r = base::Result::from_errno(x, "connect(2)");
      goto end;
    }

    ::bzero(&ss, sizeof(ss));
    sslen = sizeof(ss);
    rc = ::getsockname(fdpair.first, RISA(&ss), &sslen);
    if (rc != 0) {
      int err_no = errno;
      r = base::Result::from_errno(err_no, "getsockname(2)");
      goto end;
    }

    r = protocol->interpret(&la, type, RICSA(&ss), sslen);
    if (!r) goto end;

    ::bzero(&ss, sizeof(ss));
    sslen = sizeof(ss);
    rc = ::getpeername(fdpair.first, RISA(&ss), &sslen);
    if (rc != 0) {
      int err_no = errno;
      r = base::Result::from_errno(err_no, "getpeername(2)");
      goto end;
    }

    r = protocol->interpret(&ra, type, RICSA(&ss), sslen);
    if (!r) goto end;

    VLOG(6) << "net::FDProtocol::dial: fd=" << fdpair.first << ", "
            << "self=" << la << ", "
            << "peer=" << ra;

    r = fdconn(out, la, ra, filedesc);

  end:
    fdpair.second.unlock();
    task->finish(std::move(r));
    return base::Result();
  }
};
}  // anonymous namespace

void FDProtocol::listen(event::Task* task, ListenConn* out, const Addr& bind,
                        const base::Options& opts, AcceptFn fn) {
  CHECK(task);
  CHECK(out);
  CHECK(bind);
  CHECK(fn);
  std::string protocol = bind.protocol();
  CHECK(supports(protocol));
  if (!task->start()) return;

  ProtocolType p = bind.protocol_type();
  int domain, type, protonum;
  std::tie(domain, type, protonum) = socket_triple(protocol);
  int fdnum = ::socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protonum);
  if (fdnum == -1) {
    int err_no = errno;
    task->finish(base::Result::from_errno(err_no, "socket(2)"));
    return;
  }
  base::FD fd = base::wrapfd(fdnum);

  const auto ra = opts.get<net::Options>().reuseaddr;
  if (ra) {
    int x = 1;
    base::Result r = sockopt_reuseaddr.set(fd, &x, sizeof(x));
    r.expect_ok(__FILE__, __LINE__);
  }

  bool apply = false;
  int value;
  const auto dl = opts.get<net::Options>().duallisten;
  switch (dl) {
    case DualListen::system_default:
      break;

    case DualListen::v6mapped:
      apply = true;
      value = 0;
      break;

    case DualListen::v6only:
      apply = true;
      value = 1;
      break;

    default:
      LOG(DFATAL) << "BUG! Unknown DualListen value: "
                  << static_cast<uint8_t>(dl);
  }
  if (apply) {
    base::Result r = sockopt_ipv6_v6only.set(fd, &value, sizeof(value));
    r.expect_ok(__FILE__, __LINE__);
  }

  auto raw = bind.raw();
  int rc = ::bind(fdnum, RICSA(raw.first), raw.second);
  if (rc != 0) {
    int err_no = errno;
    task->finish(base::Result::from_errno(err_no, "bind(2)"));
    return;
  }

  sockaddr_storage ss;
  socklen_t sslen = sizeof(ss);
  rc = ::getsockname(fdnum, RISA(&ss), &sslen);
  if (rc != 0) {
    int err_no = errno;
    task->finish(base::Result::from_errno(err_no, "getsockname(2)"));
    return;
  }
  CHECK_GE(sslen, 0U);

  Addr bound;
  base::Result r = interpret(&bound, p, RICSA(&ss), sslen);
  if (!r) {
    task->finish(std::move(r));
    return;
  }

  rc = ::listen(fdnum, 1000);
  if (rc != 0) {
    int err_no = errno;
    task->finish(base::Result::from_errno(err_no, "listen(2)"));
    return;
  }

  r = fdlistenconn(out, self(), std::move(bound), std::move(fd), opts,
                   std::move(fn));
  task->finish(std::move(r));
}

void FDProtocol::dial(event::Task* task, Conn* out, const Addr& peer,
                      const Addr& bind, const base::Options& opts) {
  CHECK(task);
  CHECK(out);
  CHECK(peer);
  std::string protocol = peer.protocol();
  CHECK(supports(protocol));
  CHECK(!bind || bind.protocol() == protocol);
  if (!task->start()) return;

  ProtocolType p = peer.protocol_type();
  int domain, type, protonum;
  std::tie(domain, type, protonum) = socket_triple(protocol);
  int fdnum = ::socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protonum);
  if (fdnum == -1) {
    int err_no = errno;
    task->finish(base::Result::from_errno(err_no, "socket(2)"));
    return;
  }
  base::FD fd = base::wrapfd(fdnum);

  if (bind) {
    auto raw = bind.raw();
    int rc = ::bind(fdnum, RICSA(raw.first), raw.second);
    if (rc != 0) {
      int err_no = errno;
      task->finish(base::Result::from_errno(err_no, "bind(2)"));
      return;
    }
  }

  auto h = std::make_shared<DialHelper>(this, task, out, p, fd);
  auto closure = [h](event::Data data) { return h->handle(data); };

  auto raw = peer.raw();
redo:
  int rc = ::connect(fdnum, RICSA(raw.first), raw.second);
  if (rc == 0) {
    closure(event::Data()).ignore_ok();
    return;
  }
  int err_no = errno;
  if (err_no == EINTR) goto redo;
  base::Result r;
  if (err_no == EINPROGRESS) {
    event::Manager m = io::get_manager(opts);
    r = m.fd(&h->evt, fd, event::Set::writable_bit(), event::handler(closure));
    if (r) return;
  } else {
    r = base::Result::from_errno(err_no, "connect(2)");
  }
  task->finish(std::move(r));
}

io::Reader fdconnreader(base::FD fd) {
  CHECK(fd);
  return io::Reader(std::make_shared<FDConnReader>(std::move(fd)));
}

io::Writer fdconnwriter(base::FD fd) {
  CHECK(fd);
  return io::Writer(std::make_shared<FDConnWriter>(std::move(fd)));
}

base::Result fdconn(Conn* out, Addr la, Addr ra, base::FD fd) {
  CHECK(out);
  CHECK(la);
  CHECK(ra);
  CHECK_EQ(la.protocol(), ra.protocol());
  CHECK(fd);

  io::Reader r = fdconnreader(fd);
  io::Writer w = fdconnwriter(std::move(fd));
  auto impl = std::make_shared<FDConn>(std::move(la), std::move(ra),
                                       std::move(r), std::move(w));
  *out = Conn(std::move(impl));
  return base::Result();
}

base::Result fdlistenconn(ListenConn* out, std::shared_ptr<Protocol> pr,
                          Addr aa, base::FD fd, const base::Options& opts,
                          AcceptFn fn) {
  CHECK(out);
  CHECK(pr);
  CHECK(aa);
  CHECK(fd);
  CHECK(fn);
  auto ptr = std::make_shared<FDListenConn>(io::get_manager(opts),
                                            std::move(pr), std::move(aa),
                                            std::move(fd), std::move(fn));
  auto r = ptr->initialize();
  if (r) {
    *out = ListenConn(std::move(ptr));
  }
  return r;
}

}  // namespace net
