// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <condition_variable>
#include <mutex>
#include <thread>

#include "base/mutex.h"
#include "base/result_testing.h"
#include "event/manager.h"
#include "io/testing.h"
#include "io/util.h"

using Mock = io::MockReader::Mock;
using Verb = io::MockReader::Mock::Verb;

TEST(MockReader, EndToEnd) {
  std::mutex mu;
  std::condition_variable cv;
  bool ready = false;

  io::MockReader mr;
  const base::Options o;
  io::Reader r = io::mockreader(&mr);

  std::thread t0([&mu, &cv, &ready, &mr, &o, r] {
    mr.expect({
        Mock(Verb::write_to, "Hello, world!\n"),
    });

    auto lock = base::acquire_lock(mu);
    while (!ready) cv.wait(lock);
    lock.unlock();

    std::string out;
    io::Writer w = io::stringwriter(&out);
    event::Task task;
    std::size_t n;
    io::copy(&task, &n, w, r, o);
    event::wait(io::get_manager(o), &task);
    EXPECT_OK(task.result());
    EXPECT_EQ("Hello, world!\n", out);
  });

  std::thread t1([&mu, &cv, &ready, &mr, &o, r] {
    mr.expect({
        Mock(Verb::write_to, "", base::Result::not_implemented()),
        Mock(Verb::read, "Hello, world!\n"),
        Mock(Verb::read, "", base::Result::eof()),
    });

    auto lock = base::acquire_lock(mu);
    while (!ready) cv.wait(lock);
    lock.unlock();

    std::string out;
    io::Writer w = io::stringwriter(&out);
    event::Task task;
    std::size_t n;
    io::copy(&task, &n, w, r, o);
    event::wait(io::get_manager(o), &task);
    EXPECT_OK(task.result());
    EXPECT_EQ("Hello, world!\n", out);
  });

  mr.expect({
      Mock(Verb::close, "", base::Result()),
  });

  auto lock = base::acquire_lock(mu);
  ready = true;
  cv.notify_all();
  lock.unlock();

  t0.join();
  t1.join();

  event::Task task;
  r.close(&task, o);
  event::wait(io::get_manager(o), &task);
  EXPECT_OK(task.result());

  mr.verify();
}
