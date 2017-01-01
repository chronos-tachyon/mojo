// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/mutex.h"

#include <stdexcept>
#include <system_error>

#include "base/logging.h"

namespace base {

void RWMutex::lock() {
  Lock lck(mu_);
  ++writers_;
  while (locked_ || readers_ > 0) wcv_.wait(lck);
  locked_ = true;
}

bool RWMutex::try_lock() {
  Lock lck(mu_, std::try_to_lock);
  if (!lck.owns_lock()) return false;
  if (locked_ || readers_ > 0) return false;
  ++writers_;
  locked_ = true;
  return true;
}

void RWMutex::unlock() {
  Lock lck(mu_);
  locked_ = false;
  --writers_;
  if (writers_ == 0)
    rcv_.notify_all();
  else
    wcv_.notify_one();
}

void RWMutex::lock_read() {
  Lock lck(mu_);
  while (writers_ > 0) rcv_.wait(lck);
  ++readers_;
}

bool RWMutex::try_lock_read() {
  Lock lck(mu_, std::try_to_lock);
  if (!lck.owns_lock()) return false;
  if (writers_ > 0) return false;
  ++readers_;
  return true;
}

void RWMutex::unlock_read() {
  Lock lck(mu_);
  --readers_;
  if (writers_ > 0 && readers_ == 0) wcv_.notify_one();
}

void RLock::lock() {
  if (!ptr_)
    throw std::system_error(
        std::make_error_code(std::errc::operation_not_permitted),
        "RWMutex == nullptr");
  if (held_)
    throw std::system_error(
        std::make_error_code(std::errc::resource_deadlock_would_occur),
        "attempt to lock a locked mutex");
  ptr_->lock_read();
  held_ = true;
}

void RLock::unlock() {
  if (!held_)
    throw std::system_error(
        std::make_error_code(std::errc::operation_not_permitted),
        "RLock does not own the RWMutex");
  if (!ptr_)
    throw std::system_error(
        std::make_error_code(std::errc::operation_not_permitted),
        "RWMutex == nullptr");
  ptr_->unlock_read();
  held_ = false;
}

}  // namespace base
