// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/fake.h"

#include <sys/socket.h>

#include <algorithm>
#include <array>
#include <deque>
#include <stdexcept>
#include <thread>

#include "base/cleanup.h"
#include "base/logging.h"
#include "base/util.h"
#include "io/pipe.h"
#include "net/connfd.h"

using P = net::ProtocolType;

namespace net {

static base::Result closed_conn() {
  return base::Result::failed_precondition("net::Conn is closed");
}

static base::Result closed_listenconn() {
  return base::Result::failed_precondition("net::ListenConn is closed");
}

static bool has_prefix(const std::string& s, const std::string& prefix) {
  auto result = std::mismatch(prefix.begin(), prefix.end(), s.begin());
  return result.first == prefix.end();
}

static std::pair<bool, uint8_t> from_hex(char ch) {
  if (ch >= '0' && ch <= '9')
    return std::make_pair(true, ch - '0');
  else if (ch >= 'A' && ch <= 'F')
    return std::make_pair(true, ch - 'A' + 10);
  else if (ch >= 'a' && ch <= 'f')
    return std::make_pair(true, ch - 'a' + 10);
  else
    return std::make_pair(false, 0);
}

using ProtoList = std::vector<std::pair<std::string, ProtocolType>>;

static const ProtoList& protolist() {
  static const auto& ref = *new ProtoList{
      {"fake", P::stream},
      // {"fakegram", P::datagram},
      // {"fakepacket", P::dataseqpacket},
  };
  return ref;
}

static bool protohas(const std::string& protocol) {
  for (const auto& pair : protolist()) {
    if (pair.first == protocol) return true;
  }
  return false;
}

static bool protohas(ProtocolType p) {
  for (const auto& pair : protolist()) {
    if (pair.second == p) return true;
  }
  return false;
}

static ProtocolType protofwd(const std::string& protocol) {
  for (const auto& pair : protolist()) {
    if (pair.first == protocol) return pair.second;
  }
  throw std::logic_error("BUG: protocol not supported");
}

static std::string protorev(ProtocolType p) {
  for (const auto& pair : protolist()) {
    if (pair.second == p) return pair.first;
  }
  throw std::logic_error("BUG: ProtocolType not supported");
}

static Addr fakeaddr_locked(base::Lock& lock, FakeData* data, ProtocolType p,
                            uint32_t x);

static uint32_t u32(const Addr& addr) {
  (void)protofwd(addr.protocol());
  auto raw = addr.raw();
  CHECK_EQ(raw.second, 4U);
  const uint8_t* p = reinterpret_cast<const uint8_t*>(raw.first);
  uint32_t x = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
               (uint32_t(p[2]) << 8) | uint32_t(p[3]);
  return x;
}

static std::pair<uint32_t, uint32_t> key(uint32_t x, uint32_t y) {
  if (x > y) std::swap(x, y);
  return std::make_pair(x, y);
}

static void incref(base::Lock& lock, FakeData* data, ProtocolType p,
                   uint32_t x) {
  auto& port = data->ports[p][x];
  port.refcount++;
}

static void incref(base::Lock& lock, FakeData* data, ProtocolType p, uint32_t x,
                   uint32_t y, std::size_t n) {
  auto& ports = data->ports[p];
  auto& pairs = data->pairs[p];
  ports[x].refcount += n;
  ports[y].refcount += n;
  for (std::size_t i = 0; i < n; ++i) {
    pairs.insert(key(x, y));
  }
}

static void decref(base::Lock& lock, FakeData* data, ProtocolType p,
                   uint32_t x) {
  auto& ports = data->ports[p];
  ports[x].refcount--;
  if (ports[x].refcount == 0) ports.erase(x);
}

static void decref(base::Lock& lock, FakeData* data, ProtocolType p, uint32_t x,
                   uint32_t y, std::size_t n) {
  auto& ports = data->ports[p];
  auto& pairs = data->pairs[p];
  for (std::size_t i = 0; i < n; ++i) {
    pairs.erase(key(x, y));
  }
  ports[x].refcount -= n;
  ports[y].refcount -= n;
  if (ports[x].refcount == 0) ports.erase(x);
  if (ports[y].refcount == 0) ports.erase(y);
}

namespace {
class FakeAddr : public AddrImpl {
 public:
  explicit FakeAddr(ProtocolType p, uint32_t x) noexcept : p_(p) {
    raw_[0] = (x >> 24) & 0xff;
    raw_[1] = (x >> 16) & 0xff;
    raw_[2] = (x >> 8) & 0xff;
    raw_[3] = x & 0xff;
  }

  std::string protocol() const override { return protorev(p_); }

  ProtocolType protocol_type() const override { return p_; }

  std::string address() const override {
    static const char HEX[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string result;
    result.reserve(10);
    result.push_back('0');
    result.push_back('x');
    result.push_back(HEX[(raw_[0] >> 4) & 0xf]);
    result.push_back(HEX[raw_[0] & 0xf]);
    result.push_back(HEX[(raw_[1] >> 4) & 0xf]);
    result.push_back(HEX[raw_[1] & 0xf]);
    result.push_back(HEX[(raw_[2] >> 4) & 0xf]);
    result.push_back(HEX[raw_[2] & 0xf]);
    result.push_back(HEX[(raw_[3] >> 4) & 0xf]);
    result.push_back(HEX[raw_[3] & 0xf]);
    return result;
  }

  std::pair<const void*, std::size_t> raw() const override {
    return std::make_pair(raw_.data(), raw_.size());
  }

 private:
  ProtocolType p_;
  std::array<uint8_t, 4> raw_;
};

class FakeConn : public ConnImpl {
 public:
  FakeConn(FakeData* data, ProtocolType p, uint32_t lx, uint32_t rx,
           io::Reader r, io::Writer w) noexcept
      : data_(data),
        p_(p),
        lx_(lx),
        rx_(rx),
        r_(std::move(r)),
        w_(std::move(w)),
        closed_(false) {}

  ~FakeConn() noexcept {
    auto lock = base::acquire_lock(data_->mu);
    if (!closed_) CHECK_OK(close_impl(lock));
  }

  Addr local_addr() const override { return fakeaddr(data_, p_, lx_); }
  Addr remote_addr() const override { return fakeaddr(data_, p_, rx_); }
  io::Reader reader() override { return r_; }
  io::Writer writer() override { return w_; }

  void close(event::Task* task, const io::Options& opts) override {
    auto lock = base::acquire_lock(data_->mu);
    auto r = close_impl(lock);
    if (task->start()) task->finish(std::move(r));
  }

  void get_option(event::Task* task, SockOpt opt, void* optval,
                  unsigned int* optlen,
                  const io::Options& opts) const override {
    if (task->start()) task->finish(base::Result::not_implemented());
  }

  void set_option(event::Task* task, SockOpt opt, const void* optval,
                  unsigned int optlen, const io::Options& opts) override {
    if (task->start()) task->finish(base::Result::not_implemented());
  }

  base::Result close_impl(base::Lock& lock) {
    if (closed_) return closed_conn();
    closed_ = true;
    decref(lock, data_, p_, lx_, rx_, 1);
    lock.unlock();

    w_.close().ignore_ok();
    r_.close().ignore_ok();
    return base::Result();
  }

 private:
  FakeData* const data_;
  const ProtocolType p_;
  const uint32_t lx_;
  const uint32_t rx_;
  const io::Reader r_;
  const io::Writer w_;
  bool closed_;  // protected by data_->mu
};

class FakeListenConn : public ListenConnImpl {
 private:
  struct Pending {
    event::Task* task;
    Conn* out;
    uint32_t x;
    Options opts;

    Pending(event::Task* t, Conn* o, uint32_t x, Options opts) noexcept
        : task(t),
          out(o),
          x(x),
          opts(std::move(opts)) {}
  };

 public:
  FakeListenConn(FakeData* data, ProtocolType p, uint32_t ax, Options o,
                 AcceptFn fn) noexcept : data_(data),
                                         p_(p),
                                         x_(ax),
                                         fn_(std::move(fn)),
                                         closed_(false),
                                         accepting_(false) {}

  ~FakeListenConn() noexcept {
    auto lock = base::acquire_lock(data_->mu);
    if (!closed_) CHECK_OK(close_impl(lock));
  }

  Addr listen_addr() const override { return fakeaddr(data_, p_, x_); }

  void start(event::Task* task, const io::Options& opts) override {
    if (!task->start()) return;
    auto lock = base::acquire_lock(data_->mu);
    if (closed_) {
      task->finish(closed_listenconn());
      return;
    }
    accepting_ = true;
    task->finish_ok();
    process(lock);
  }

  void stop(event::Task* task, const io::Options& opts) override {
    if (!task->start()) return;
    auto lock = base::acquire_lock(data_->mu);
    if (closed_) {
      task->finish(closed_listenconn());
      return;
    }
    accepting_ = false;
    task->finish_ok();
  }

  void close(event::Task* task, const io::Options& opts) override {
    auto lock = base::acquire_lock(data_->mu);
    base::Result r = close_impl(lock);
    if (task->start()) task->finish(std::move(r));
  }

  void get_option(event::Task* task, SockOpt opt, void* optval,
                  unsigned int* optlen,
                  const io::Options& opts) const override {
    if (task->start()) task->finish(base::Result::not_implemented());
  }

  void set_option(event::Task* task, SockOpt opt, const void* optval,
                  unsigned int optlen, const io::Options& opts) override {
    if (task->start()) task->finish(base::Result::not_implemented());
  }

  base::Result close_impl(base::Lock& lock) {
    if (closed_) {
      return closed_listenconn();
    }
    accepting_ = false;
    closed_ = true;
    data_->ports[p_][x_].listener = nullptr;
    decref(lock, data_, p_, x_);
    std::deque<Pending> q = std::move(q_);
    q_.clear();
    for (const auto& pending : q) {
      decref(lock, data_, p_, x_, pending.x, 2);
    }
    lock.unlock();

    for (const auto& pending : q) {
      pending.task->finish_cancel();  // TODO: better error
    }
    return base::Result();
  }

  void do_dial(base::Lock& lock, event::Task* t, Conn* o, uint32_t x,
               Options opts) {
    VLOG(4) << "enqueueing dial from 0x" << std::hex << x;
    q_.emplace_back(t, o, x, std::move(opts));
    process(lock);
  }

  void process(base::Lock& lock) {
    if (!accepting_) return;
    while (!q_.empty()) {
      process_one(lock, std::move(q_.front()));
      q_.pop_front();
    }
  }

  void process_one(base::Lock& lock, Pending&& pending) {
    std::size_t num_refs = 2;
    auto cleanup = base::cleanup([this, &lock, &pending, &num_refs] {
      if (num_refs < 1) return;
      decref(lock, data_, p_, x_, pending.x, num_refs);
    });

    VLOG(4) << "processing dial from 0x" << std::hex << pending.x;

    // In the below code, the two connections are:
    // - a: the accepted connection
    // - b: the dialed connection

    io::Reader atob_r, btoa_r;
    io::Writer atob_w, btoa_w;
    io::make_pipe(&atob_r, &atob_w);
    io::make_pipe(&btoa_r, &btoa_w);

    Conn a(std::make_shared<FakeConn>(data_, p_, x_, pending.x,
                                      std::move(btoa_r), std::move(atob_w)));
    num_refs--;

    Conn b(std::make_shared<FakeConn>(data_, p_, pending.x, x_,
                                      std::move(atob_r), std::move(btoa_w)));
    num_refs--;

    fn_(std::move(a));
    *pending.out = std::move(b);
    pending.task->finish_ok();
  }

 private:
  FakeData* const data_;
  const ProtocolType p_;
  const uint32_t x_;
  const AcceptFn fn_;
  std::deque<Pending> q_;  // protected by data_->mu
  bool closed_;            // protected by data_->mu
  bool accepting_;         // protected by data_->mu
};

class FakeProtocol : public Protocol {
 public:
  FakeProtocol(FakeData* data) noexcept : data_(data) {}

  bool interprets(int family) const override {
    return false;  // FakeProtcol does not have an AF_* constant
  }

  base::Result interpret(Addr* out, ProtocolType p, const sockaddr* sa,
                         int len) const override {
    return base::Result::not_implemented();
  }

  bool supports(const std::string& protocol) const override {
    return protohas(protocol);
  }

  base::Result parse(Addr* out, const std::string& protocol,
                     const std::string& address) const override {
    auto p = protofwd(protocol);
    if (!has_prefix(address, "0x") && !has_prefix(address, "0X"))
      return base::Result::not_found("address does not begin with '0x'");
    auto it = address.begin() + 2, end = address.end();
    while (it != end && *it == '0') ++it;
    if (std::distance(it, end) > 8)
      return base::Result::not_found("address is too large");
    uint32_t x = 0;
    while (it != end) {
      auto pair = from_hex(*it);
      if (!pair.first)
        return base::Result::not_found("address contains non-hex digit");
      x = (x << 4) | pair.second;
      ++it;
    }
    *out = fakeaddr(data_, p, x);
    return base::Result();
  }

  void resolve(event::Task* task, std::vector<Addr>* out,
               const std::string& protocol, const std::string& address,
               const Options& opts) override {
    if (!task->start()) return;
    auto p = protofwd(protocol);
    auto lock = base::acquire_lock(data_->mu);
    auto it = data_->names.find(address);
    if (it == data_->names.end()) {
      task->finish(base::Result::not_found());
      return;
    }
    for (uint32_t x : it->second) {
      out->push_back(fakeaddr_locked(lock, data_, p, x));
    }
    task->finish_ok();
  }

  void listen(event::Task* task, ListenConn* out, const Addr& bind,
              const Options& opts, AcceptFn fn) override {
    CHECK_NOTNULL(out);
    CHECK(protohas(bind.protocol()));

    if (!task->start()) return;
    auto lock = base::acquire_lock(data_->mu);

    ProtocolType p = bind.protocol_type();
    uint32_t x = u32(bind);
    incref(lock, data_, p, x);
    auto cleanup =
        base::cleanup([this, &lock, p, x] { decref(lock, data_, p, x); });

    auto& port = data_->ports[p][x];
    if (port.listener) {
      task->finish(base::Result::from_errno(EADDRINUSE, "in-process bind"));
    }
    if (port.refcount > 1 && !opts.reuseaddr()) {
      task->finish(base::Result::from_errno(EADDRINUSE, "in-process bind"));
    }

    auto impl =
        std::make_shared<FakeListenConn>(data_, p, x, opts, std::move(fn));
    std::weak_ptr<FakeListenConn> weak(impl);
    port.listener = [weak](base::Lock& lock, event::Task* t, Conn* o,
                           uint32_t x, Options opts) {
      auto strong = weak.lock();
      if (!strong) {
        t->finish_cancel();  // TODO: better error
        return;
      }
      strong->do_dial(lock, t, o, x, std::move(opts));
    };
    *out = ListenConn(std::move(impl));
    task->finish_ok();

    VLOG(2) << p << " listen at 0x" << std::hex << x;

    cleanup.cancel();
  }

  void dial(event::Task* task, Conn* out, const Addr& peer, const Addr& bind,
            const Options& opts) override {
    CHECK_NOTNULL(task);
    CHECK_NOTNULL(out);
    CHECK(protohas(peer.protocol()));
    CHECK(!bind || bind.protocol() == peer.protocol());

    if (!task->start()) return;
    auto lock = base::acquire_lock(data_->mu);

    ProtocolType p = peer.protocol_type();
    auto& ports = data_->ports[p];

    uint32_t x;
    if (bind) {
      x = u32(bind);
    } else {
      bool found = false;
      for (uint32_t proposed = 0xffff0000U; proposed != 0; ++proposed) {
        auto it = ports.find(proposed);
        if (it == ports.end() || it->second.refcount == 0) {
          found = true;
          x = proposed;
          break;
        }
      }
      if (!found) {
        task->finish(
            base::Result::from_errno(EADDRNOTAVAIL, "in-process bind"));
        return;
      }
    }

    uint32_t y = u32(peer);

    incref(lock, data_, p, x, y, 2);
    auto cleanup = base::cleanup(
        [this, &lock, p, x, y] { decref(lock, data_, p, x, y, 2); });

    auto& pairs = data_->pairs[p];
    if (x == y || pairs.count(key(x, y)) > 2) {
      task->finish(base::Result::from_errno(EADDRINUSE, "in-process bind"));
      return;
    }

    auto& bindport = ports[x];
    if (bindport.refcount > 2 && !opts.reuseaddr()) {
      task->finish(base::Result::from_errno(EADDRINUSE, "in-process bind"));
      return;
    }

    auto& peerport = ports[y];
    if (!peerport.listener) {
      task->finish(base::Result::from_errno(ECONNREFUSED, "in-process dial"));
      return;
    }

    VLOG(2) << p << " dial to 0x" << std::hex << y << " from 0x" << x;

    peerport.listener(lock, task, out, x, opts);
    cleanup.cancel();
  }

 private:
  FakeData* const data_;
};
}  // anonymous namespace

static Addr fakeaddr_locked(base::Lock& lock, FakeData* data, ProtocolType p,
                            uint32_t x) {
  auto& port = data->ports[p][x];
  if (!port.addr) port.addr = Addr(std::make_shared<FakeAddr>(p, x));
  return port.addr;
}

Addr fakeaddr(FakeData* data, ProtocolType p, uint32_t x) {
  CHECK_NOTNULL(data);
  if (!protohas(p)) throw std::invalid_argument("ProtocolType not supported");
  auto lock = base::acquire_lock(data->mu);
  return fakeaddr_locked(lock, data, p, x);
}

Addr fakeaddr(ProtocolType p, uint32_t x) {
  if (!protohas(p)) throw std::invalid_argument("ProtocolType not supported");
  return Addr(std::make_shared<FakeAddr>(p, x));
}

std::shared_ptr<Protocol> fakeprotocol(FakeData* data) {
  CHECK_NOTNULL(data);
  return std::make_shared<FakeProtocol>(data);
}

}  // namespace net
