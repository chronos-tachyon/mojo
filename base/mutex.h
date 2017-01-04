// base/mutex.h - Mutex and lock implementations
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_MUTEX_H
#define BASE_MUTEX_H

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace base {

using Lock = std::unique_lock<std::mutex>;

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

  // Acquire/Release the mutex in exclusive writer mode.
  void lock();
  bool try_lock();
  void unlock();

  // Acquire/Release the mutex in shared reader mode.
  void lock_read();
  bool try_lock_read();
  void unlock_read();

 private:
  mutable std::mutex mu_;        // protect the fields below
  std::condition_variable wcv_;  // notify waiting writers
  std::condition_variable rcv_;  // notify waiting readers
  std::size_t readers_;          // # of read locks
  std::size_t writers_;          // # of (active + pending) write locks
  bool locked_;                  // true iff a writer holds the lock
};

// WLock is a customization of unique_lock for using RWMutex in writer mode.
using WLock = std::unique_lock<RWMutex>;

// RLock is an RAII class for locking/unlocking a RWMutex in reader mode.
class RLock {
 public:
  // RLock usually locks its mutex upon construction.
  explicit RLock(RWMutex& rwmu) : ptr_(&rwmu), held_(false) { lock(); }
  RLock(RWMutex& rwmu, std::defer_lock_t) noexcept : ptr_(&rwmu),
                                                     held_(false) {}
  RLock(RWMutex& rwmu, std::try_to_lock_t)
      : ptr_(&rwmu), held_(ptr_->try_lock_read()) {}
  RLock(RWMutex& rwmu, std::adopt_lock_t) : ptr_(&rwmu), held_(true) {}

  // RLock is default constructible.
  RLock() noexcept : ptr_(nullptr), held_(false) {}

  // RLock is not copyable.
  RLock(const RLock&) = delete;
  RLock& operator=(const RLock&) = delete;

  // RLock is moveable.
  RLock(RLock&& x) noexcept : ptr_(x.ptr_), held_(x.held_) {
    x.ptr_ = nullptr;
    x.held_ = false;
  }
  RLock& operator=(RLock&& x) noexcept {
    if (held_) unlock();
    ptr_ = x.ptr_;
    held_ = x.held_;
    x.ptr_ = nullptr;
    x.held_ = false;
    return *this;
  }

  // RLock unlocks its mutex upon destruction, but only if it holds the lock.
  ~RLock() noexcept {
    if (held_) unlock();
  }

  // Locks the mutex.
  // - It is an error to call this if the mutex is already locked.
  // - It is an error to call this if no mutex is associated with this lock.
  void lock();

  // Unlocks the mutex.
  // - It is an error to call this if the mutex is not locked.
  void unlock();

  // Swaps this RLock with another.
  void swap(RLock& x) noexcept {
    std::swap(ptr_, x.ptr_);
    std::swap(held_, x.held_);
  }

  // Releases ownership of the lock, without unlocking it.
  // - After this call, the RLock is in the default constructed state.
  RWMutex* release() noexcept {
    RWMutex* rwmu = ptr_;
    ptr_ = nullptr;
    held_ = false;
    return rwmu;
  }

  // Accessor methods for checking the state of the lock.
  RWMutex* mutex() const noexcept { return ptr_; }
  bool owns_lock() const noexcept { return held_; }
  explicit operator bool() const noexcept { return held_; }

 private:
  RWMutex* ptr_;
  bool held_;
};

inline void swap(RLock& a, RLock& b) noexcept { a.swap(b); }

inline Lock acquire_lock(std::mutex& mu) { return Lock(mu); }
inline WLock acquire_write(RWMutex& rwmu) { return WLock(rwmu); }
inline RLock acquire_read(RWMutex& rwmu) { return RLock(rwmu); }

}  // namespace base

#endif  // BASE_MUTEX_H
