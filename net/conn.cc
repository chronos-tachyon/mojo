// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/conn.h"

#include "base/logging.h"

namespace net {

namespace {
struct GetOptHelper {
  const unsigned int expected;
  unsigned int actual;

  GetOptHelper(unsigned int size) noexcept : expected(size), actual(size) {}

  base::Result done() {
    CHECK_EQ(expected, actual);
    delete this;
    return base::Result();
  }
};
}  // anonymous namespace

void Conn::assert_valid() const { CHECK(ptr_) << ": net::Conn is empty"; }

void Conn::get_option(event::Task* task, SockOpt opt, void* optval,
                      unsigned int* optlen, const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(optval);
  CHECK_NOTNULL(optlen);
  assert_valid();
  ptr_->get_option(task, opt, optval, optlen, opts);
}

void Conn::get_int_option(event::Task* task, SockOpt opt, int* value,
                          const base::Options& opts) const {
  CHECK_NOTNULL(value);
  auto* helper = new GetOptHelper(sizeof(*value));
  get_option(task, opt, value, &helper->actual, opts);
  auto closure = [helper] { return helper->done(); };
  task->on_finished(event::callback(closure));
}

void Conn::get_tv_option(event::Task* task, SockOpt opt, struct timeval* value,
                         const base::Options& opts) const {
  CHECK_NOTNULL(value);
  auto* helper = new GetOptHelper(sizeof(*value));
  get_option(task, opt, value, &helper->actual, opts);
  auto closure = [helper] { return helper->done(); };
  task->on_finished(event::callback(closure));
}

void Conn::set_option(event::Task* task, SockOpt opt, const void* optval,
                      unsigned int optlen, const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(optval);
  assert_valid();
  ptr_->set_option(task, opt, optval, optlen, opts);
}

void Conn::set_int_option(event::Task* task, SockOpt opt, int value,
                          const base::Options& opts) const {
  auto* ptr = new int(value);
  set_option(task, opt, ptr, sizeof(value), opts);
  auto closure = [ptr] {
    delete ptr;
    return base::Result();
  };
  task->on_finished(event::callback(closure));
}

void Conn::set_tv_option(event::Task* task, SockOpt opt, struct timeval value,
                         const base::Options& opts) const {
  auto* ptr = new struct timeval;
  ::memcpy(ptr, &value, sizeof(value));
  set_option(task, opt, ptr, sizeof(value), opts);
  auto closure = [ptr] {
    delete ptr;
    return base::Result();
  };
  task->on_finished(event::callback(closure));
}

base::Result Conn::close(const base::Options& opts) const {
  event::Task task;
  close(&task, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result Conn::get_option(SockOpt opt, void* optval, unsigned int* optlen,
                              const base::Options& opts) const {
  event::Task task;
  get_option(&task, opt, optval, optlen, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result Conn::get_int_option(SockOpt opt, int* value,
                                  const base::Options& opts) const {
  CHECK_NOTNULL(value);
  unsigned int value_len = sizeof(*value);
  base::Result r = get_option(opt, value, &value_len, opts);
  CHECK_EQ(value_len, sizeof(*value));
  return r;
}

base::Result Conn::get_tv_option(SockOpt opt, struct timeval* value,
                                 const base::Options& opts) const {
  CHECK_NOTNULL(value);
  unsigned int value_len = sizeof(*value);
  base::Result r = get_option(opt, value, &value_len, opts);
  CHECK_EQ(value_len, sizeof(*value));
  return r;
}

base::Result Conn::set_option(SockOpt opt, const void* optval,
                              unsigned int optlen,
                              const base::Options& opts) const {
  event::Task task;
  set_option(&task, opt, optval, optlen, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result Conn::set_int_option(SockOpt opt, int value,
                                  const base::Options& opts) const {
  return set_option(opt, &value, sizeof(value), opts);
}

base::Result Conn::set_tv_option(SockOpt opt, struct timeval value,
                                 const base::Options& opts) const {
  return set_option(opt, &value, sizeof(value), opts);
}

void ListenConn::assert_valid() const {
  CHECK(ptr_) << ": net::ListenConn is empty";
}

void ListenConn::get_option(event::Task* task, SockOpt opt, void* optval,
                            unsigned int* optlen,
                            const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(optval);
  CHECK_NOTNULL(optlen);
  assert_valid();
  ptr_->get_option(task, opt, optval, optlen, opts);
}

void ListenConn::get_int_option(event::Task* task, SockOpt opt, int* value,
                                const base::Options& opts) const {
  auto* helper = new GetOptHelper(sizeof(*value));
  get_option(task, opt, value, &helper->actual, opts);
  auto closure = [helper] { return helper->done(); };
  task->on_finished(event::callback(closure));
}

void ListenConn::get_tv_option(event::Task* task, SockOpt opt,
                               struct timeval* value,
                               const base::Options& opts) const {
  auto* helper = new GetOptHelper(sizeof(*value));
  get_option(task, opt, value, &helper->actual, opts);
  auto closure = [helper] { return helper->done(); };
  task->on_finished(event::callback(closure));
}

void ListenConn::set_option(event::Task* task, SockOpt opt, const void* optval,
                            unsigned int optlen,
                            const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(optval);
  assert_valid();
  ptr_->set_option(task, opt, optval, optlen, opts);
}

void ListenConn::set_int_option(event::Task* task, SockOpt opt, int value,
                                const base::Options& opts) const {
  auto* ptr = new int(value);
  set_option(task, opt, ptr, sizeof(value), opts);
  auto closure = [ptr] {
    delete ptr;
    return base::Result();
  };
  task->on_finished(event::callback(closure));
}

void ListenConn::set_tv_option(event::Task* task, SockOpt opt,
                               struct timeval value,
                               const base::Options& opts) const {
  auto* ptr = new struct timeval;
  ::memcpy(ptr, &value, sizeof(value));
  set_option(task, opt, ptr, sizeof(value), opts);
  auto closure = [ptr] {
    delete ptr;
    return base::Result();
  };
  task->on_finished(event::callback(closure));
}

base::Result ListenConn::start(const base::Options& opts) const {
  event::Task task;
  start(&task, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result ListenConn::stop(const base::Options& opts) const {
  event::Task task;
  stop(&task, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result ListenConn::close(const base::Options& opts) const {
  event::Task task;
  close(&task, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result ListenConn::get_option(SockOpt opt, void* optval,
                                    unsigned int* optlen,
                                    const base::Options& opts) const {
  event::Task task;
  get_option(&task, opt, optval, optlen, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result ListenConn::get_int_option(SockOpt opt, int* value,
                                        const base::Options& opts) const {
  CHECK_NOTNULL(value);
  unsigned int value_len = sizeof(*value);
  base::Result r = get_option(opt, value, &value_len, opts);
  CHECK_EQ(value_len, sizeof(*value));
  return r;
}

base::Result ListenConn::get_tv_option(SockOpt opt, struct timeval* value,
                                       const base::Options& opts) const {
  CHECK_NOTNULL(value);
  unsigned int value_len = sizeof(*value);
  base::Result r = get_option(opt, value, &value_len, opts);
  CHECK_EQ(value_len, sizeof(*value));
  return r;
}

base::Result ListenConn::set_option(SockOpt opt, const void* optval,
                                    unsigned int optlen,
                                    const base::Options& opts) const {
  event::Task task;
  set_option(&task, opt, optval, optlen, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result ListenConn::set_int_option(SockOpt opt, int value,
                                        const base::Options& opts) const {
  return set_option(opt, &value, sizeof(value), opts);
}

base::Result ListenConn::set_tv_option(SockOpt opt, struct timeval value,
                                       const base::Options& opts) const {
  return set_option(opt, &value, sizeof(value), opts);
}

}  // namespace net
