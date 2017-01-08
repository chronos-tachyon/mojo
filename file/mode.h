// file/mode.h - File open modes
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef FILE_MODE_H
#define FILE_MODE_H

#include <algorithm>
#include <cstdint>
#include <ostream>

namespace file {

class Mode {
 private:
  static constexpr uint16_t bit_r = (1U << 0);
  static constexpr uint16_t bit_w = (1U << 1);
  static constexpr uint16_t bit_a = (1U << 2);
  static constexpr uint16_t bit_c = (1U << 3);
  static constexpr uint16_t bit_x = (1U << 4);
  static constexpr uint16_t bit_t = (1U << 5);

 public:
  static constexpr Mode read_bit() noexcept { return Mode(bit_r); }
  static constexpr Mode write_bit() noexcept { return Mode(bit_w); }
  static constexpr Mode append_bit() noexcept { return Mode(bit_a); }
  static constexpr Mode create_bit() noexcept { return Mode(bit_c); }
  static constexpr Mode exclusive_bit() noexcept { return Mode(bit_x); }
  static constexpr Mode truncate_bit() noexcept { return Mode(bit_t); }

  // Opens the existing file in read-only mode.
  // Equivalent to fopen(3)'s "r".
  static constexpr Mode ro_mode() noexcept { return read_bit(); }

  // Opens the existing file in read-write mode.
  // Equivalent to fopen(3)'s "r+".
  static constexpr Mode rw_mode() noexcept { return read_bit() | write_bit(); }

  // Creates the file in write-only mode; truncates it if it exists.
  // Equivalent to fopen(3)'s "w".
  static constexpr Mode create_truncate_wo_mode() noexcept {
    return write_bit() | create_bit() | truncate_bit();
  }

  // Creates the file in read-write mode; truncates it if it exists.
  // Equivalent to fopen(3)'s "w".
  static constexpr Mode create_truncate_rw_mode() noexcept {
    return read_bit() | create_truncate_wo_mode();
  }

  // Creates the file in write-only mode for appending.
  // Equivalent to fopen(3)'s "a".
  static constexpr Mode create_ao_mode() noexcept {
    return write_bit() | create_bit() | append_bit();
  }

  // Creates the file in write-only mode for appending.
  // Equivalent to fopen(3)'s "a+".
  static constexpr Mode create_ra_mode() noexcept {
    return read_bit() | create_ao_mode();
  }

  // Opens the existing file in write-only mode.
  static constexpr Mode wo_mode() noexcept { return write_bit(); }

  // Opens the existing file in write-only mode for appending.
  static constexpr Mode ao_mode() noexcept { return write_bit() | append_bit(); }

  // Opens the existing file in read-write mode for appending.
  static constexpr Mode ra_mode() noexcept { return read_bit() | ao_mode(); }

  // Opens the existing file in write-only mode, truncating it.
  static constexpr Mode truncate_wo_mode() noexcept {
    return write_bit() | truncate_bit();
  }

  // Creates the file in write-only mode; the file must not exist.
  static constexpr Mode create_exclusive_wo_mode() noexcept {
    return write_bit() | create_bit() | exclusive_bit();
  }

  // Creates the file in read-write mode; the file must not exist.
  static constexpr Mode create_exclusive_rw_mode() noexcept {
    return read_bit() | create_exclusive_wo_mode();
  }

  Mode(const char* str) noexcept;
  constexpr explicit Mode(uint16_t bits) noexcept : bits_(bits) {}
  constexpr Mode() noexcept : bits_(0) {}
  constexpr Mode(const Mode&) noexcept = default;
  constexpr Mode(Mode&&) noexcept = default;
  Mode& operator=(const Mode&) noexcept = default;
  Mode& operator=(Mode&&) noexcept = default;

  Mode& operator&=(Mode other) noexcept { return (*this = *this & other); }
  Mode& operator|=(Mode other) noexcept { return (*this = *this | other); }
  Mode& operator^=(Mode other) noexcept { return (*this = *this ^ other); }

  void clear() noexcept { bits_ = 0; }
  void swap(Mode& other) noexcept { std::swap(bits_, other.bits_); }
  bool valid() const noexcept;

  constexpr explicit operator bool() const noexcept { return bits_ != 0; }
  constexpr explicit operator uint16_t() const noexcept { return bits_; }

  constexpr Mode operator~() const noexcept { return Mode(~bits_); }
  constexpr Mode operator&(Mode other) const noexcept {
    return Mode(bits_ & other.bits_);
  }
  constexpr Mode operator|(Mode other) const noexcept {
    return Mode(bits_ | other.bits_);
  }
  constexpr Mode operator^(Mode other) const noexcept {
    return Mode(bits_ ^ other.bits_);
  }

  constexpr bool has(uint16_t mask) const noexcept {
    return (bits_ & mask) != 0;
  }
  constexpr bool read() const noexcept { return has(bit_r); }
  constexpr bool write() const noexcept { return has(bit_w); }
  constexpr bool append() const noexcept { return has(bit_a); }
  constexpr bool create() const noexcept { return has(bit_c); }
  constexpr bool exclusive() const noexcept { return has(bit_x); }
  constexpr bool truncate() const noexcept { return has(bit_t); }

  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept { return 6; }
  std::string as_string() const;

  friend constexpr bool operator==(Mode a, Mode b) noexcept;

 private:
  uint16_t bits_;
};

inline void swap(Mode& a, Mode& b) noexcept { a.swap(b); }
inline constexpr bool operator==(Mode a, Mode b) noexcept {
  return a.bits_ == b.bits_;
}
inline constexpr bool operator!=(Mode a, Mode b) noexcept { return !(a == b); }

inline std::ostream& operator<<(std::ostream& o, Mode m) {
  return (o << '"' << m.as_string() << '"');
}

}  // namespace file

#endif  // FILE_MODE_H
