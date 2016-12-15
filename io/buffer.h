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

namespace io {

// Helper function for rounding up to the next power of 2.
__attribute__((const)) inline std::size_t next_power_of_two(
    std::size_t n) noexcept {
  static constexpr std::size_t MAX = ~std::size_t(0);
  static constexpr std::size_t MAXPOW2 = (MAX >> 1) + 1;
  if (n > MAXPOW2) return MAX;
  std::size_t x = 1;
  while (x < n) x <<= 1;
  return x;
}

// A ConstBuffer points to a block of read-only memory.
// The ConstBuffer does *NOT* own the memory.
class ConstBuffer {
 public:
  constexpr ConstBuffer() noexcept : ptr_(nullptr), len_(0) {}

  constexpr ConstBuffer(const char* ptr, std::size_t len)
      : ptr_(len > 0 ? ptr : nullptr), len_(len) {}

  ConstBuffer(const std::string& str) : ConstBuffer(str.data(), str.size()) {}

  ConstBuffer(const std::vector<char>& vec)
      : ConstBuffer(vec.data(), vec.size()) {}

  template <std::size_t N>
  ConstBuffer(const std::array<char, N>& arr)
      : ConstBuffer(arr.data(), arr.size()) {}

  constexpr const char* data() const noexcept { return ptr_; }
  constexpr std::size_t size() const noexcept { return len_; }
  explicit constexpr operator bool() const noexcept { return ptr_ != nullptr; }

 private:
  const char* ptr_;
  std::size_t len_;
};

// A Buffer points to a block of read-write memory.
// The Buffer does *NOT* own the memory.
class Buffer {
 public:
  constexpr Buffer() noexcept : ptr_(nullptr), len_(0) {}

  constexpr Buffer(char* ptr, std::size_t len)
      : ptr_(len > 0 ? ptr : nullptr), len_(len) {}

  Buffer(std::vector<char>& vec) : Buffer(vec.data(), vec.size()) {}

  template <std::size_t N>
  Buffer(std::array<char, N>& arr) : Buffer(arr.data(), arr.size()) {}

  constexpr char* data() const noexcept { return ptr_; }
  constexpr std::size_t size() const noexcept { return len_; }
  explicit constexpr operator bool() const noexcept { return ptr_ != nullptr; }

  constexpr operator ConstBuffer() const noexcept {
    return ConstBuffer(ptr_, len_);
  }

 private:
  char* ptr_;
  std::size_t len_;
};

// An OwnedBuffer points to (and owns) a block of read-write memory.
class OwnedBuffer {
 public:
  // OwnedBuffer can allocate its own buffer.
  explicit OwnedBuffer(std::size_t len);

  // OwnedBuffer can adopt ownership of an existing char array.
  OwnedBuffer(std::unique_ptr<char[]> ptr, std::size_t len) noexcept;

  // OwnedBuffer can be default constructed as an empty buffer.
  OwnedBuffer() noexcept : ptr_(nullptr), len_(0) {}

  // OwnedBuffer is moveable.
  OwnedBuffer(OwnedBuffer&& x) noexcept : ptr_(std::move(x.ptr_)),
                                          len_(x.len_) {
    x.len_ = 0;
  }
  OwnedBuffer& operator=(OwnedBuffer&& x) noexcept {
    ptr_ = std::move(x.ptr_);
    len_ = x.len_;
    x.len_ = 0;
    return *this;
  }

  // OwnedBuffer is not copyable.
  OwnedBuffer(const OwnedBuffer&) = delete;
  OwnedBuffer& operator=(const OwnedBuffer&) = delete;

  char* data() noexcept { return ptr_.get(); }
  const char* data() const noexcept { return ptr_.get(); }
  std::size_t size() const noexcept { return len_; }
  explicit operator bool() const noexcept { return !!ptr_; }

  operator Buffer() noexcept { return Buffer(data(), size()); }
  operator ConstBuffer() const noexcept { return ConstBuffer(data(), size()); }

 private:
  std::unique_ptr<char[]> ptr_;
  std::size_t len_;
};

struct null_pool_t {
  constexpr null_pool_t() noexcept = default;
};

static constexpr null_pool_t null_pool = {};

// A BufferPool is a thread-safe pool of OwnedBuffer objects.
// - All OwnedBuffer objects in the pool have the same size.
class BufferPool {
 public:
  // BufferPools are normally constructed with a fixed buffer size.
  BufferPool(std::size_t size) noexcept : size_(next_power_of_two(size)),
                                          guts_(std::make_shared<Guts>()) {}

  // BufferPools can be constructed with a null pool. A pool in such a state
  // will always allocate for |take()| and will always release for |give()|.
  // - Use |reserve(0)| to force a non-null pool.
  BufferPool(std::size_t size, null_pool_t) noexcept
      : size_(next_power_of_two(size)),
        guts_() {}

  // BufferPools are default constructible, but not very useful in that state.
  BufferPool() noexcept : BufferPool(0, null_pool) {}

  // BufferPools are copyable and moveable.
  // Copies point to the same pool.
  BufferPool(const BufferPool&) noexcept = default;
  BufferPool(BufferPool&&) noexcept = default;
  BufferPool& operator=(const BufferPool&) noexcept = default;
  BufferPool& operator=(BufferPool&&) noexcept = default;

  // Returns the size of the buffers in this pool.
  std::size_t buffer_size() const noexcept { return size_; }

  // Returns the number of buffers in the pool.
  std::size_t pool_size() const noexcept;

  // Returns the maximum number of buffers in the pool.
  std::size_t pool_max() const noexcept;
  void set_pool_max(std::size_t n) noexcept;

  // Returns true iff this pool is non-null.
  explicit operator bool() const noexcept { return !!guts_; }

  // Swaps this pool with another.
  void swap(BufferPool& x) noexcept {
    using std::swap;
    swap(size_, x.size_);
    swap(guts_, x.guts_);
  }

  // Frees all buffers in this pool.
  void flush();

  // Hints that there should be at least |count| buffers in the pool.
  // - Side effect: upgrades a null pool to non-null
  void reserve(std::size_t count);

  // Returns |buf| to the pool.
  // - |buf.size()| must match |buffer_size()|
  void give(OwnedBuffer buf);

  // Returns a buffer from the pool if one is available, or else allocates one.
  OwnedBuffer take();

 private:
  struct Guts {
    std::mutex mu;
    std::vector<OwnedBuffer> pool;
    std::size_t max;

    Guts() noexcept : max(16) {}
  };

  std::size_t size_;
  std::shared_ptr<Guts> guts_;
};

}  // namespace io

#endif  // IO_BUFFER_H
