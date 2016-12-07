// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/buffer.h"

#include <cstring>

#include "base/logging.h"
#include "base/util.h"

namespace io {

static char* alloc(std::size_t len) {
  char* ptr = nullptr;
  if (len > 0) {
    ptr = new char[len];
    ::bzero(ptr, len);
  }
  return ptr;
}

OwnedBuffer::OwnedBuffer(std::size_t len) : ptr_(alloc(len)), len_(len) {}

OwnedBuffer::OwnedBuffer(std::unique_ptr<char[]> ptr, std::size_t len) noexcept
    : ptr_(std::move(ptr)),
      len_(len) {
  CHECK(len_ == 0 || ptr_.get() != nullptr);
  if (len_ == 0) ptr_ = nullptr;
  if (len_ > 0) ::bzero(ptr_.get(), len_);
}

std::size_t BufferPool::pool_size() const noexcept {
  if (!guts_) return 0;
  auto lock = base::acquire_lock(guts_->mu);
  return guts_->pool.size();
}

std::size_t BufferPool::pool_max() const noexcept {
  if (!guts_) return 0;
  auto lock = base::acquire_lock(guts_->mu);
  return guts_->max;
}

void BufferPool::set_pool_max(std::size_t n) noexcept {
  if (!guts_) guts_ = std::make_shared<Guts>();
  auto lock = base::acquire_lock(guts_->mu);
  guts_->max = n;
  while (guts_->pool.size() > guts_->max) {
    guts_->pool.pop_back();
  }
}

void BufferPool::flush() {
  if (!guts_) return;
  auto lock = base::acquire_lock(guts_->mu);
  guts_->pool.clear();
}

void BufferPool::reserve(std::size_t count) {
  if (!guts_) guts_ = std::make_shared<Guts>();
  auto lock = base::acquire_lock(guts_->mu);
  if (count > guts_->max) count = guts_->max;
  while (guts_->pool.size() < count) {
    guts_->pool.push_back(OwnedBuffer(size_));
  }
}

void BufferPool::give(OwnedBuffer buf) {
  if (buf.size() != size_) {
    LOG(DFATAL) << "BUG: This io::BufferPool only accepts " << size_
                << "-byte buffers, but was given a " << buf.size()
                << "-byte buffer!";
    return;
  }
  if (!guts_) return;
  auto lock = base::acquire_lock(guts_->mu);
  guts_->pool.push_back(std::move(buf));
}

OwnedBuffer BufferPool::take() {
  OwnedBuffer buf;
  if (guts_) {
    auto lock = base::acquire_lock(guts_->mu);
    if (!guts_->pool.empty()) {
      buf = std::move(guts_->pool.back());
      guts_->pool.pop_back();
    }
  }
  if (!buf) buf = OwnedBuffer(size_);
  return buf;
}

}  // namespace io
