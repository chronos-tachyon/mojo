// base/util.h - Miscellaneous small utility functions
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_UTIL_H
#define BASE_UTIL_H

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace base {

using Lock = std::unique_lock<std::mutex>;

inline Lock acquire_lock(std::mutex& mu) { return Lock(mu); }

// RWMutex is a readers-writers lock with a strong writers bias.
// It's intended for protecting frequently-read, rarely-updated data.
class RWMutex {
 public:
  // RWMutexes are default constructible.
  RWMutex() : readers_(0), writers_(0), locked_(false) {}

  // RWMutexes are neither copyable nor moveable
  RWMutex(const RWMutex&) = delete;
  RWMutex(RWMutex&&) = delete;
  RWMutex& operator=(const RWMutex&) = delete;
  RWMutex& operator=(RWMutex&&) = delete;

  void lock() {
    auto lock = acquire_lock(mu_);
    ++writers_;
    while (locked_ || readers_ > 0) wcv_.wait(lock);
    locked_ = true;
  }

  bool try_lock() {
    Lock lock(mu_, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    if (locked_ || readers_ > 0) return false;
    ++writers_;
    locked_ = true;
    return true;
  }

  void unlock() {
    auto lock = acquire_lock(mu_);
    locked_ = false;
    --writers_;
    if (writers_ == 0)
      rcv_.notify_all();
    else
      wcv_.notify_one();
  }

  void lock_read() {
    auto lock = acquire_lock(mu_);
    while (writers_ > 0) rcv_.wait(lock);
    ++readers_;
  }

  bool try_lock_read() {
    Lock lock(mu_, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    if (writers_ > 0) return false;
    ++readers_;
  }

  void unlock_read() {
    auto lock = acquire_lock(mu_);
    --readers_;
    if (writers_ > 0 && readers_ == 0) wcv_.notify_one();
  }

 private:
  std::mutex mu_;
  std::condition_variable wcv_;
  std::condition_variable rcv_;
  std::size_t readers_;  // # of read locks
  std::size_t writers_;  // # of (active + pending) write locks
  bool locked_;          // True iff a writer holds the lock
};

class RLock {
 public:
  // RLocks are default constructible.
  RLock() noexcept : mu_(nullptr), held_(false) {}

  // RLocks are moveable.
  RLock(RLock&& x) noexcept : mu_(x.mu_), held_(x.held_) { x.held_ = false; }

  // Acquire a read lock.
  explicit RLock(RWMutex& mu) : mu_(&mu), held_(true) { mu_->lock_read(); }
  RLock(RWMutex& mu, std::defer_lock_t) noexcept : mu_(&mu), held_(false) {}
  RLock(RWMutex& mu, std::try_to_lock_t) : mu_(&mu), held_(mu_->try_lock_read()) {}
  RLock(RWMutex& mu, std::adopt_lock_t) : mu_(&mu), held_(true) {}

  // RLocks are moveable.
  RLock& operator=(RLock&& x) noexcept {
    if (held_) mu_->unlock_read();
    mu_ = x.mu_;
    held_ = x.held_;
    x.held_ = false;
    return *this;
  }

  // RLocks release their mutexes upon destruction.
  ~RLock() {
    if (held_) mu_->unlock_read();
  }

  // RLocks are not copyable.
  RLock(const RLock&) = delete;
  RLock& operator=(const RLock&) = delete;

  void swap(RLock& x) noexcept {
    using std::swap;
    swap(mu_, x.mu_);
    swap(held_, x.held_);
  }

  RWMutex* release() noexcept {
    RWMutex* mu = mu_;
    held_ = false;
    mu_ = nullptr;
    return mu;
  }

  RWMutex* mutex() const noexcept { return mu_; }
  bool owns_lock() const noexcept { return held_; }
  explicit operator bool() const noexcept { return held_; }

 private:
  RWMutex* mu_;
  bool held_;
};

using WLock = std::unique_lock<RWMutex>;

inline RLock acquire_read(RWMutex& mu) { return RLock(mu); }
inline WLock acquire_write(RWMutex& mu) { return WLock(mu); }

}  // namespace base

#endif  // BASE_UTIL_H
