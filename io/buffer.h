// io/buffer.h - Reuseable scratch buffers
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_BUFFER_H
#define IO_BUFFER_H

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "base/strings.h"

namespace io {

// A ConstBuffer points to a block of read-only memory.
// The ConstBuffer does *NOT* own the memory.
class ConstBuffer {
 public:
  constexpr ConstBuffer(const char* ptr, std::size_t len) noexcept
      : data_(ptr),
        size_(len) {}

  constexpr ConstBuffer() noexcept : ConstBuffer(nullptr, 0) {}

  constexpr ConstBuffer(base::StringPiece sp) noexcept
      : ConstBuffer(sp.data(), sp.size()) {}

  ConstBuffer(const std::string& str) : ConstBuffer(str.data(), str.size()) {}

  ConstBuffer(const std::vector<char>& vec)
      : ConstBuffer(vec.data(), vec.size()) {}

  template <std::size_t N>
  ConstBuffer(const std::array<char, N>& arr)
      : ConstBuffer(arr.data(), arr.size()) {}

  explicit constexpr operator bool() const noexcept { return size_ != 0; }
  constexpr const char* data() const noexcept { return data_; }
  constexpr std::size_t size() const noexcept { return size_; }

 private:
  const char* data_;
  std::size_t size_;
};

// A Buffer points to a block of read-write memory.
// The Buffer does *NOT* own the memory.
class Buffer {
 public:
  constexpr Buffer(char* ptr, std::size_t len) noexcept : data_(ptr),
                                                          size_(len) {}

  constexpr Buffer() noexcept : Buffer(nullptr, 0) {}

  Buffer(std::vector<char>& vec) : Buffer(vec.data(), vec.size()) {}

  template <std::size_t N>
  Buffer(std::array<char, N>& arr) : Buffer(arr.data(), arr.size()) {}

  explicit constexpr operator bool() const noexcept { return size_ != 0; }
  constexpr char* data() const noexcept { return data_; }
  constexpr std::size_t size() const noexcept { return size_; }

  constexpr operator ConstBuffer() const noexcept {
    return ConstBuffer(data_, size_);
  }

 private:
  char* data_;
  std::size_t size_;
};

// An OwnedBuffer points to (and owns) a block of read-write memory.
class OwnedBuffer {
 public:
  // OwnedBuffer can allocate its own buffer.
  explicit OwnedBuffer(std::size_t len);

  // OwnedBuffer can adopt ownership of an existing char array.
  OwnedBuffer(std::unique_ptr<char[]> ptr, std::size_t len) noexcept;

  // OwnedBuffer can be default constructed as an empty buffer.
  OwnedBuffer() noexcept : data_(nullptr), size_(0) {}

  // OwnedBuffer is moveable.
  OwnedBuffer(OwnedBuffer&& x) noexcept : data_(std::move(x.data_)),
                                          size_(x.size_) {
    x.size_ = 0;
  }
  OwnedBuffer& operator=(OwnedBuffer&& x) noexcept {
    data_ = std::move(x.data_);
    size_ = x.size_;
    x.size_ = 0;
    return *this;
  }

  // OwnedBuffer is not copyable.
  OwnedBuffer(const OwnedBuffer&) = delete;
  OwnedBuffer& operator=(const OwnedBuffer&) = delete;

  char* data() noexcept { return data_.get(); }
  const char* data() const noexcept { return data_.get(); }
  std::size_t size() const noexcept { return size_; }
  explicit operator bool() const noexcept { return !!data_; }

  operator Buffer() noexcept { return Buffer(data(), size()); }
  operator ConstBuffer() const noexcept { return ConstBuffer(data(), size()); }

 private:
  std::unique_ptr<char[]> data_;
  std::size_t size_;
};

// A Pool is a thread-safe pool of OwnedBuffer objects.
// - All OwnedBuffer objects in the pool have the same size.
class Pool {
 public:
  // Pool is constructed with a fixed buffer size.
  Pool(std::size_t size, std::size_t max_buffers) noexcept;

  // Pool is neither copyable nor moveable.
  Pool(const Pool&) = delete;
  Pool(Pool&&) = delete;
  Pool& operator=(const Pool&) = delete;
  Pool& operator=(Pool&&) = delete;

  // Returns the size of each buffer in this pool.
  std::size_t buffer_size() const noexcept { return size_; }

  // Returns the maximum number of buffers in this pool.
  std::size_t max() const noexcept { return max_; }

  // Returns the current number of buffers in this pool.
  std::size_t size() const noexcept;

  // Frees all buffers in this pool.
  void flush() noexcept;

  // Hints that there should be at least |count| buffers in the pool.
  void reserve(std::size_t count);

  // Returns |buf| to the pool.
  // - |buf.size()| must match |buffer_size()|
  void give(OwnedBuffer buf) noexcept;

  // Returns a buffer from the pool if one is available, or else allocates one.
  OwnedBuffer take();

 private:
  const std::size_t size_;
  const std::size_t max_;
  mutable std::mutex mu_;
  std::vector<OwnedBuffer> vec_;
};

using PoolPtr = std::shared_ptr<Pool>;

inline PoolPtr make_pool(std::size_t buffer_size, std::size_t max_buffers) {
  return std::make_shared<Pool>(buffer_size, max_buffers);
}

}  // namespace io

#endif  // IO_BUFFER_H
