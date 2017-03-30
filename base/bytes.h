// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_BYTES_H
#define BASE_BYTES_H

#include <cstdint>
#include <string>
#include <vector>

#include "base/strings.h"

namespace base {

// Represents a view into an immutable byte buffer.
class Bytes {
 public:
  static constexpr std::size_t npos = ~std::size_t(0);

  constexpr Bytes(std::nullptr_t = nullptr, std::size_t = 0) noexcept
      : data_(nullptr),
        size_(0) {}

  constexpr Bytes(const uint8_t* ptr, std::size_t len) noexcept : data_(ptr),
                                                                  size_(len) {}

  Bytes(const std::vector<uint8_t>& v) noexcept : data_(v.data()),
                                                  size_(v.size()) {}

  constexpr explicit Bytes(const char* ptr, std::size_t len) noexcept
      : data_(reinterpret_cast<const uint8_t*>(ptr)),
        size_(len) {}

  explicit Bytes(const std::vector<char>& v) noexcept
      : data_(reinterpret_cast<const uint8_t*>(v.data())),
        size_(v.size()) {}

  explicit Bytes(const std::string& s) noexcept
      : data_(reinterpret_cast<const uint8_t*>(s.data())),
        size_(s.size()) {}

  constexpr Bytes(const Bytes&) noexcept = default;
  constexpr Bytes(Bytes&&) noexcept = default;
  Bytes& operator=(const Bytes&) noexcept = default;
  Bytes& operator=(Bytes&&) noexcept = default;

  template <typename... Args>
  void assign(Args&&... args) noexcept {
    *this = Bytes(std::forward<Args>(args)...);
  }

  constexpr const uint8_t* data() const noexcept { return data_; }
  constexpr std::size_t size() const noexcept { return size_; }

  constexpr const char* chars() const noexcept {
    return reinterpret_cast<const char*>(data_);
  }

  constexpr const uint8_t* begin() const noexcept { return data_; }
  constexpr const uint8_t* end() const noexcept { return data_ + size_; }

  constexpr Bytes substring(std::size_t pos, std::size_t len = npos) {
    return (pos >= size_)
               ? Bytes(data_ + size_, 0)
               : ((len > size_ - pos) ? Bytes(data_ + pos, size_ - pos)
                                      : Bytes(data_ + pos, len));
  }

  constexpr base::StringPiece as_stringpiece() const noexcept {
    return base::StringPiece(chars(), size());
  }

  std::string as_string() const { return std::string(chars(), size()); }

  operator std::vector<uint8_t>() const {
    return std::vector<uint8_t>(begin(), end());
  }

 private:
  const uint8_t* data_;
  std::size_t size_;
};

// Represents a view into a mutable byte buffer.
class MutableBytes {
 public:
  static constexpr std::size_t npos = ~std::size_t(0);

  MutableBytes(std::nullptr_t = nullptr, std::size_t = 0) noexcept
      : data_(nullptr),
        size_(0) {}

  MutableBytes(uint8_t* ptr, std::size_t len) noexcept : data_(ptr),
                                                         size_(len) {}

  MutableBytes(std::vector<uint8_t>& v) noexcept : data_(v.data()),
                                                   size_(v.size()) {}

  explicit MutableBytes(char* ptr, std::size_t len) noexcept
      : data_(reinterpret_cast<uint8_t*>(ptr)),
        size_(len) {}

  explicit MutableBytes(std::vector<char>& v) noexcept
      : data_(reinterpret_cast<uint8_t*>(v.data())),
        size_(v.size()) {}

  MutableBytes(const MutableBytes&) noexcept = default;
  MutableBytes(MutableBytes&&) noexcept = default;
  MutableBytes& operator=(const MutableBytes&) noexcept = default;
  MutableBytes& operator=(MutableBytes&&) noexcept = default;

  template <typename... Args>
  void assign(Args&&... args) noexcept {
    *this = MutableBytes(std::forward<Args>(args)...);
  }

  uint8_t* data() const noexcept { return data_; }
  std::size_t size() const noexcept { return size_; }

  char* chars() const noexcept { return reinterpret_cast<char*>(data_); }

  uint8_t* begin() const noexcept { return data_; }
  uint8_t* end() const noexcept { return data_ + size_; }

  MutableBytes substring(std::size_t pos, std::size_t len = npos) {
    return (pos >= size_)
               ? MutableBytes(data_ + size_, 0)
               : ((len > size_ - pos) ? MutableBytes(data_ + pos, size_ - pos)
                                      : MutableBytes(data_ + pos, len));
  }

  base::StringPiece as_stringpiece() const noexcept {
    return base::StringPiece(chars(), size());
  }

  std::string as_string() const { return std::string(chars(), size()); }

  operator std::vector<uint8_t>() const {
    return std::vector<uint8_t>(begin(), end());
  }

  operator Bytes() noexcept { return Bytes(data_, size_); }

 private:
  uint8_t* data_;
  std::size_t size_;
};

}  // namespace base

#endif  // BASE_BYTES_H
