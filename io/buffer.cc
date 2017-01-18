// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/buffer.h"

#include <cstring>

#include "base/debug.h"
#include "base/logging.h"
#include "base/mutex.h"

namespace io {

__attribute__((const)) inline std::size_t next_power_of_two(
    std::size_t n) noexcept {
  static constexpr std::size_t MAX = ~std::size_t(0);
  static constexpr std::size_t MAXPOW2 = (MAX >> 1) + 1;
  if (n > MAXPOW2) return MAX;
  std::size_t x = 1;
  while (x < n) x <<= 1;
  return x;
}

static char* alloc(std::size_t len) {
  char* ptr = nullptr;
  if (len > 0) {
    ptr = new char[len];
    ::bzero(ptr, len);
  }
  return ptr;
}

OwnedBuffer::OwnedBuffer(std::size_t len) : data_(alloc(len)), size_(len) {}

OwnedBuffer::OwnedBuffer(std::unique_ptr<char[]> ptr, std::size_t len) noexcept
    : data_(std::move(ptr)),
      size_(len) {
  CHECK(size_ == 0 || data_);
  if (size_ == 0) data_ = nullptr;
  if (size_ > 0) ::bzero(data_.get(), size_);
}

Pool::Pool(std::size_t size, std::size_t max_buffers) noexcept
    : size_(next_power_of_two(size)),
      max_(max_buffers) {
  CHECK_GT(size, 0U);
  vec_.reserve(max_);
}

std::size_t Pool::size() const noexcept {
  auto lock = base::acquire_lock(mu_);
  return vec_.size();
}

void Pool::flush() noexcept {
  auto lock = base::acquire_lock(mu_);
  vec_.clear();
}

void Pool::reserve(std::size_t count) {
  if (count > max_) count = max_;
  auto lock = base::acquire_lock(mu_);
  while (vec_.size() < count) vec_.push_back(OwnedBuffer(size_));
}

void Pool::give(OwnedBuffer buf) noexcept {
  ::bzero(buf.data(), buf.size());
  if (buf.size() != size_) {
    LOG(DFATAL) << "BUG: This io::Pool only accepts " << size_
                << "-byte buffers, but was given a " << buf.size()
                << "-byte buffer!";
    return;
  }
  auto lock = base::acquire_lock(mu_);
  if (vec_.size() < max_) vec_.push_back(std::move(buf));
}

OwnedBuffer Pool::take() {
  OwnedBuffer buf;
  auto lock = base::acquire_lock(mu_);
  if (vec_.empty()) {
    buf = OwnedBuffer(size_);
  } else {
    buf = std::move(vec_.back());
    vec_.pop_back();
  }
  return buf;
}

}  // namespace io
