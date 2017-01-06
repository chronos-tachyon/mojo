// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/testing.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "base/logging.h"
#include "base/result_testing.h"
#include "event/manager.h"
#include "event/task.h"
#include "gtest/gtest.h"

using RC = base::ResultCode;

namespace net {

namespace {

class Countdown {
 public:
  Countdown() noexcept : n_(0) { task_.start(); }

  void add(std::size_t count = 1) noexcept {
    auto lock = base::acquire_lock(mu_);
    n_ += count;
  }

  void done(std::size_t count = 1) noexcept {
    auto lock = base::acquire_lock(mu_);
    CHECK_GE(n_, count);
    n_ -= count;
    if (n_ == 0) task_.finish_ok();
  }

  event::Task* task() noexcept { return &task_; }

 private:
  event::Task task_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::size_t n_;
};

struct AcceptHelper {
  Countdown* const talk;
  const std::size_t id;
  const net::Conn conn;
  const base::Options options;
  event::Task task;
  std::size_t n;
  char buf[64];

  AcceptHelper(Countdown* t, std::size_t id, net::Conn c,
               base::Options opts) noexcept : talk(t),
                                              id(id),
                                              conn(std::move(c)),
                                              options(std::move(opts)),
                                              n(0) {
    ::bzero(buf, sizeof(buf));
    VLOG(0) << "net::AcceptHelper::AcceptHelper";
  }

  ~AcceptHelper() noexcept {
    VLOG(0) << "net::AcceptHelper::~AcceptHelper";
    talk->done();
  }

  event::DispatcherPtr dispatcher() const noexcept {
    return io::get_manager(options).dispatcher();
  }

  void run() {
    LOG(INFO) << "server #" << id << ": read";
    conn.reader().read(&task, buf, &n, 1, sizeof(buf), options);
    auto closure = [this] { return read_complete(); };
    task.on_finished(dispatcher(), event::callback(closure));
  }

  base::Result read_complete() {
    std::string str(buf, n);
    LOG(INFO) << "server #" << id << ": read '" << str << "' complete";
    auto r = task.result();
    bool eof = false;
    if (r.code() == RC::END_OF_FILE) {
      r.reset();
      eof = true;
    }
    EXPECT_OK(r);
    if (eof || !r) {
      bomb_out();
      return base::Result();
    }

    std::size_t len = n;
    if (len > 0) {
      std::size_t i = 0, j = len - 1;
      while (j > i) {
        std::swap(buf[i], buf[j]);
        ++i, --j;
      }
    }

    task.reset();
    str.assign(buf, len);
    LOG(INFO) << "server #" << id << ": write '" << str << "'";
    conn.writer().write(&task, &n, buf, len, options);
    auto closure = [this] { return write_complete(); };
    task.on_finished(dispatcher(), event::callback(closure));
    return base::Result();
  }

  base::Result write_complete() {
    LOG(INFO) << "server #" << id << ": write complete";
    EXPECT_OK(task.result());
    if (!task.result()) {
      bomb_out();
      return base::Result();
    }
    task.reset();
    LOG(INFO) << "server #" << id << ": read";
    conn.reader().read(&task, buf, &n, 1, sizeof(buf), options);
    auto closure = [this] { return read_complete(); };
    task.on_finished(dispatcher(), event::callback(closure));
    return base::Result();
  }

  void bomb_out() {
    task.reset();
    LOG(INFO) << "server #" << id << ": close";
    conn.close(&task, options);
    auto closure = [this] { return close_complete(); };
    task.on_finished(dispatcher(), event::callback(closure));
  }

  base::Result close_complete() {
    LOG(INFO) << "server #" << id << ": close complete";
    EXPECT_OK(task.result());
    dispatcher()->dispose(this);
    return base::Result();
  }
};

struct TestHelper {
  Countdown* const dial;
  Countdown* const talk;
  const std::size_t id;
  const std::string send;
  const std::string recv;
  const base::Options options;
  event::Task task;
  net::Conn conn;
  std::size_t n;
  char buf[64];

  TestHelper(Countdown* d, Countdown* t, std::size_t id, std::string s,
             std::string r, base::Options o) noexcept : dial(d),
                                                        talk(t),
                                                        id(id),
                                                        send(std::move(s)),
                                                        recv(std::move(r)),
                                                        options(std::move(o)),
                                                        n(0) {
    ::bzero(buf, sizeof(buf));
    VLOG(0) << "client #" << id << ": net::TestHelper::TestHelper";
  }

  ~TestHelper() noexcept {
    VLOG(0) << "client #" << id << ": net::TestHelper::~TestHelper";
    talk->done();
  }

  event::DispatcherPtr dispatcher() const noexcept {
    return io::get_manager(options).dispatcher();
  }

  void run(Protocol* pr, Addr peer) {
    LOG(INFO) << "client #" << id << ": dial";
    pr->dial(&task, &conn, peer, net::Addr(), options);
    auto closure = [this] { return dial_complete(); };
    task.on_finished(dispatcher(), event::callback(closure));
  }

  base::Result dial_complete() {
    LOG(INFO) << "client #" << id << ": dial complete";
    dial->done();
    EXPECT_OK(task.result());
    if (!task.result()) {
      bomb_out();
      return base::Result();
    }
    task.reset();
    LOG(INFO) << "client #" << id << ": write '" << send << "'";
    conn.writer().write(&task, &n, send.data(), send.size(), options);
    auto closure = [this] { return write_complete(); };
    task.on_finished(dispatcher(), event::callback(closure));
    return base::Result();
  }

  base::Result write_complete() {
    LOG(INFO) << "client #" << id << ": write complete";
    EXPECT_OK(task.result());
    EXPECT_EQ(send.size(), n);
    if (!task.result()) {
      bomb_out();
      return base::Result();
    }
    task.reset();
    LOG(INFO) << "client #" << id << ": read";
    conn.reader().read(&task, buf, &n, 1, sizeof(buf), options);
    auto closure = [this] { return read_complete(); };
    task.on_finished(dispatcher(), event::callback(closure));
    return base::Result();
  }

  base::Result read_complete() {
    std::string str(buf, n);
    LOG(INFO) << "client #" << id << ": read '" << str << "' complete";
    EXPECT_OK(task.result());
    EXPECT_EQ(recv, str);
    bomb_out();
    return base::Result();
  }

  void bomb_out() {
    task.reset();
    LOG(INFO) << "client #" << id << ": close";
    conn.close(&task, options);
    auto closure = [this] { return close_complete(); };
    task.on_finished(dispatcher(), event::callback(closure));
  }

  base::Result close_complete() {
    LOG(INFO) << "client #" << id << ": close complete";
    EXPECT_OK(task.result());
    dispatcher()->dispose(this);
    return base::Result();
  }
};
}  // anonymous namespace

static void TestListenAndDial_Common(std::shared_ptr<Protocol> pr, Addr addr,
                                     const event::ManagerOptions& mo,
                                     const char* name) {
  std::atomic<std::size_t> last_id(0);
  Countdown dial;
  Countdown talk;

  LOG(INFO) << "[new:" << name << "]";
  base::log_flush();

  event::Manager m;
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options opts;
  opts.get<io::Options>().manager = m;

  auto acceptfn = [&last_id, &dial, &talk, &opts](net::Conn c) {
    std::size_t id = last_id.fetch_add(1) + 1;
    LOG(INFO) << "server #" << id << ": accept";
    dial.done();
    auto* helper = new AcceptHelper(&talk, id, std::move(c), opts);
    helper->run();
  };

  event::Task task;
  net::ListenConn l;

  LOG(INFO) << "[listener-create:" << name << "]";
  base::log_flush();
  pr->listen(&task, &l, addr, opts, acceptfn);
  event::wait(m, &task);
  EXPECT_OK(task.result());

  addr = l.listen_addr();

  LOG(INFO) << "[listener-accept:" << name << "]";
  base::log_flush();
  EXPECT_OK(l.start(opts));

  LOG(INFO) << "[clients-create:" << name << "]";
  for (auto t : {
           std::make_tuple(1, "0123", "3210"),
           std::make_tuple(2, "@ABC", "CBA@"),
           std::make_tuple(3, "DEFG", "GFED"),
       }) {
    dial.add(2);
    talk.add(2);
    auto* ptr = new TestHelper(&dial, &talk, std::get<0>(t), std::get<1>(t),
                               std::get<2>(t), opts);
    ptr->run(pr.get(), addr);
  }

  LOG(INFO) << "[dial-wait:" << name << "]";
  event::wait(m, dial.task());

  LOG(INFO) << "[listener-close:" << name << "]";
  base::log_flush();
  EXPECT_OK(l.close(opts));

  LOG(INFO) << "[talk-wait:" << name << "]";
  event::wait(m, talk.task());

  LOG(INFO) << "[shutdown:" << name << "]";
  base::log_flush();
  m.shutdown();

  LOG(INFO) << "[end:" << name << "]";
  base::log_flush();
}

static void TestListenAndDial_Async(std::shared_ptr<Protocol> p, Addr addr) {
  event::ManagerOptions mo;
  mo.set_async_mode();
  TestListenAndDial_Common(p, addr, mo, "async");
}

static void TestListenAndDial_SingleThreaded(std::shared_ptr<Protocol> p,
                                             Addr addr) {
  event::ManagerOptions mo;
  mo.set_minimal_threaded_mode();
  TestListenAndDial_Common(p, addr, mo, "single-threaded");
}

static void TestListenAndDial_MultiThreaded(std::shared_ptr<Protocol> p,
                                            Addr addr) {
  event::ManagerOptions mo;
  mo.set_threaded_mode();
  mo.set_num_pollers(2);
  mo.dispatcher().set_num_workers(4);
  TestListenAndDial_Common(p, addr, mo, "multi-threaded");
}

void TestListenAndDial(std::shared_ptr<Protocol> p, Addr addr) {
  CHECK_NOTNULL(p);
  CHECK(addr);
  CHECK(p->supports(addr.protocol()));
  TestListenAndDial_Async(p, addr);
  TestListenAndDial_SingleThreaded(p, addr);
  TestListenAndDial_MultiThreaded(p, addr);
}

}  // namespace net

static void init() __attribute__((constructor));
static void init() { base::log_stderr_set_level(VLOG_LEVEL(6)); }
