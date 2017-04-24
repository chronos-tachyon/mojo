// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CRYPTO_SUBTLE_H
#define CRYPTO_SUBTLE_H

#include <cstdint>
#include <stdexcept>

namespace crypto {
namespace subtle {

bool consttime_eq(const uint8_t* a, const uint8_t* b, std::size_t len) noexcept;

void* secure_allocate(std::size_t len);
void secure_deallocate(void* ptr, std::size_t len);

template <typename T>
struct SecureAllocator {
  using value_type = T;

  constexpr SecureAllocator() noexcept = default;
  constexpr SecureAllocator(const SecureAllocator&) noexcept = default;
  constexpr SecureAllocator(SecureAllocator&&) noexcept = default;
  SecureAllocator& operator=(const SecureAllocator&) noexcept = default;
  SecureAllocator& operator=(SecureAllocator&&) noexcept = default;

  template <typename U>
  constexpr SecureAllocator(const SecureAllocator<U>&) noexcept {}

  static constexpr std::size_t ONE =
      (alignof(T) > sizeof(T))
          ? alignof(T)
          : ((sizeof(T) + alignof(T) - 1) / alignof(T)) * alignof(T);

  static constexpr std::size_t array_size(std::size_t n) {
    return (n > SIZE_MAX / ONE)
               ? (throw std::overflow_error("allocation overflow"), SIZE_MAX)
               : (n * ONE);
  }

  T* allocate(std::size_t n) {
    return reinterpret_cast<T*>(secure_allocate(array_size(n)));
  }

  void deallocate(T* ptr, std::size_t n) {
    secure_deallocate(ptr, array_size(n));
  }
};

template <typename T>
struct SecureMemory {
  T* pointer;

  template <typename... Args>
  SecureMemory(Args&&... args) {
    SecureAllocator<T> allocator;
    pointer = allocator.allocate(1);
    try {
      new (pointer) T(std::forward<Args>(args)...);
    } catch (...) {
      allocator.deallocate(pointer, 1);
      throw;
    }
  }

  ~SecureMemory() {
    SecureAllocator<T> allocator;
    try {
      pointer->~T();
    } catch (...) {
      allocator.deallocate(pointer, 1);
      throw;
    }
    allocator.deallocate(pointer, 1);
  }

  T& operator*() const noexcept {
    return *pointer;
  }

  T* operator->() const noexcept {
    return pointer;
  }

  T* get() const noexcept {
    return pointer;
  }
};

}  // namespace subtle
}  // namespace crypto

#endif  // CRYPTO_SUBTLE_H
