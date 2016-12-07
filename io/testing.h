// io/testing.h - Tools for I/O in unit tests
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_TESTING_H
#define IO_TESTING_H

#include <sys/types.h>

#include <cstring>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "io/reader.h"
#include "io/writer.h"

namespace io {

pid_t gettid() noexcept;

struct IsOK {
  bool operator()(const base::Result& r) const noexcept { return !!r; }
};

class mock_violation : public std::exception {
 public:
  mock_violation(const char* what) noexcept : what_(what ? what : "") {}
  mock_violation() noexcept : mock_violation(nullptr) {}
  mock_violation(const mock_violation&) noexcept = default;
  mock_violation(mock_violation&&) noexcept = default;
  mock_violation& operator=(const mock_violation&) noexcept = default;
  mock_violation& operator=(mock_violation&&) noexcept = default;
  const char* what() const noexcept override { return what_; }

 private:
  const char* what_;
};

class MockReader : public ReaderImpl {
 public:
  struct Mock {
    enum class Verb : uint8_t {
      read = 0,
      write_to = 1,
      close = 2,
    };
    using Pred = std::function<bool(base::Result)>;

    Verb verb;
    std::string data;
    base::Result result;
    Pred pred;

    Mock(Verb v, std::string d = std::string(), base::Result r = base::Result(),
         Pred p = IsOK()) noexcept : verb(v),
                                     data(std::move(d)),
                                     result(std::move(r)),
                                     pred(std::move(p)) {}
  };

  MockReader(io::Options o) noexcept : ReaderImpl(std::move(o)), blksz_(4096) {}

  void set_block_size(std::size_t n) noexcept {
    auto lock = base::acquire_lock(mu_);
    blksz_ = n;
  }

  void expect(std::initializer_list<Mock> il) {
    auto lock = base::acquire_lock(mu_);
    auto& threadq = map_[gettid()];
    threadq.vec.insert(threadq.vec.end(), il.begin(), il.end());
  }

  void expect(Mock mock) { expect({mock}); }

  void verify() const noexcept {
    auto lock = base::acquire_lock(mu_);
    for (const auto& pair : map_) {
      const auto& threadq = pair.second;
      if (threadq.index < threadq.vec.size())
        throw mock_violation("unmet expectations");
    }
  }

  void read(event::Task* task, char* out, std::size_t* n, std::size_t min,
            std::size_t max) noexcept override {
    if (!prologue(task, out, n, min, max)) return;
    auto mock = next();
    if (mock.verb != Mock::Verb::read)
      throw mock_violation("did not expect read()");
    if (mock.data.size() < min && mock.result.ok())
      throw mock_violation("mock.data too short");
    if (mock.data.size() > max) throw mock_violation("mock.data too long");
    ::memcpy(out, mock.data.data(), mock.data.size());
    *n = mock.data.size();
    task->finish(mock.result);
  }

  void write_to(event::Task* task, std::size_t* n, std::size_t max,
                const Writer& w) noexcept override {
    if (!prologue(task, n, max, w)) return;
    auto mock = next();
    if (mock.verb != Mock::Verb::write_to)
      throw mock_violation("did not expect write_to()");
    if (mock.data.size() > max) throw mock_violation("mock.data too long");
    event::Task subtask;
    task->add_subtask(&subtask);
    w.write(&subtask, n, mock.data.data(), mock.data.size());
    if (!mock.pred(subtask.result()))
      throw mock_violation("mock.pred returned false");
    task->finish(mock.result);
  }

  void close(event::Task* task) noexcept override {
    if (!prologue(task)) return;
    auto mock = next();
    if (mock.verb != Mock::Verb::close)
      throw mock_violation("did not expect close()");
    task->finish(mock.result);
  }

  std::size_t ideal_block_size() const noexcept override { return blksz_; }

 private:
  struct Queue {
    std::vector<Mock> vec;
    std::size_t index;

    Queue() noexcept : index(0) {}
    Mock next() {
      if (index >= vec.size()) throw mock_violation("no expectation");
      return vec[index++];
    }
  };

  Mock next() {
    auto tid = gettid();
    auto lock = base::acquire_lock(mu_);
    return map_[tid].next();
  }

  mutable std::mutex mu_;
  std::map<pid_t, Queue> map_;
  std::size_t blksz_;
};

struct NoOpDeleter {
  void operator()(void*) {}
};

inline Reader mockreader(MockReader* mr) {
  return Reader(std::shared_ptr<ReaderImpl>(mr, NoOpDeleter()));
}

}  // namespace io

#endif  // IO_TESTING_H
