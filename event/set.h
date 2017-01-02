// event/set.h - Sets of event types
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef EVENT_SET_H
#define EVENT_SET_H

#include <algorithm>
#include <ostream>

namespace event {

// A Set is a collection of boolean flags, representing the types of events
// which are flagged as interesting or observed.
//
// It is a value type; you should treat it just like you would an integer.
//
class Set {
 private:
  enum bits {
    bit_readable = (1U << 0),
    bit_writable = (1U << 1),
    bit_priority = (1U << 2),
    bit_hangup = (1U << 3),
    bit_error = (1U << 4),
    bit_signal = (1U << 5),
    bit_timer = (1U << 6),
    bit_event = (1U << 7),
  };

  explicit constexpr Set(uint8_t bits) noexcept : bits_(bits) {}
  constexpr bool has(uint8_t bit) const noexcept { return (bits_ & bit) != 0; }
  constexpr Set with(uint8_t bit, bool value) const noexcept {
    return Set(value ? (bits_ | bit) : (bits_ & ~bit));
  }
  Set& set(uint8_t bit, bool value) noexcept {
    if (value)
      bits_ |= bit;
    else
      bits_ &= ~bit;
    return *this;
  }

 public:
  // Set is default constructible, copyable, and moveable.
  // Thse are guaranteed to be noexcept and (where applicable) constexpr.
  constexpr Set() noexcept : bits_(0) {}
  constexpr Set(const Set&) noexcept = default;
  constexpr Set(Set&&) noexcept = default;
  Set& operator=(const Set&) noexcept = default;
  Set& operator=(Set&&) noexcept = default;

  // Check if any of the event flags are present.
  constexpr bool empty() const noexcept { return bits_ == 0; }
  explicit constexpr operator bool() const noexcept { return !empty(); }

  // Check the values of individual event flags.
  constexpr bool readable() const noexcept { return has(bit_readable); }
  constexpr bool writable() const noexcept { return has(bit_writable); }
  constexpr bool priority() const noexcept { return has(bit_priority); }
  constexpr bool hangup() const noexcept { return has(bit_hangup); }
  constexpr bool error() const noexcept { return has(bit_error); }
  constexpr bool signal() const noexcept { return has(bit_signal); }
  constexpr bool timer() const noexcept { return has(bit_timer); }
  constexpr bool event() const noexcept { return has(bit_event); }

  // Return a new Set that has the given <flag, value>.
  constexpr Set with_readable(bool value = true) const noexcept {
    return with(bit_readable, value);
  }
  constexpr Set with_writable(bool value = true) const noexcept {
    return with(bit_writable, value);
  }
  constexpr Set with_priority(bool value = true) const noexcept {
    return with(bit_priority, value);
  }
  constexpr Set with_hangup(bool value = true) const noexcept {
    return with(bit_hangup, value);
  }
  constexpr Set with_error(bool value = true) const noexcept {
    return with(bit_error, value);
  }
  constexpr Set with_signal(bool value = true) const noexcept {
    return with(bit_signal, value);
  }
  constexpr Set with_timer(bool value = true) const noexcept {
    return with(bit_timer, value);
  }
  constexpr Set with_event(bool value = true) const noexcept {
    return with(bit_event, value);
  }

  // Perform set arithmetic using the bitwise operators.
  constexpr Set operator~() const noexcept { return Set(~bits_); }
  constexpr Set operator&(Set b) const noexcept { return Set(bits_ & b.bits_); }
  constexpr Set operator|(Set b) const noexcept { return Set(bits_ | b.bits_); }
  constexpr Set operator^(Set b) const noexcept { return Set(bits_ ^ b.bits_); }

  // Equality and comparison are defined.
  friend constexpr bool operator==(const Set& a, const Set& b) noexcept {
    return a.bits_ == b.bits_;
  }
  friend constexpr bool operator<(const Set& a, const Set& b) noexcept {
    return a.bits_ < b.bits_;
  }

  // Reset all flags to false.
  void clear() { bits_ = 0; }

  // Swap this Set of flags with another.
  void swap(Set& b) noexcept { std::swap(bits_, b.bits_); }

  // In-place variants of the set arithmetic bitwise operators.
  Set& operator&=(Set b) noexcept { return (*this = Set(bits_ & b.bits_)); }
  Set& operator|=(Set b) noexcept { return (*this = Set(bits_ | b.bits_)); }
  Set& operator^=(Set b) noexcept { return (*this = Set(bits_ ^ b.bits_)); }

  // Modify this Set to have the given <flag, value>.
  Set& set_readable(bool value = true) noexcept {
    return set(bit_readable, value);
  }
  Set& set_writable(bool value = true) noexcept {
    return set(bit_writable, value);
  }
  Set& set_priority(bool value = true) noexcept {
    return set(bit_priority, value);
  }
  Set& set_hangup(bool value = true) noexcept { return set(bit_hangup, value); }
  Set& set_error(bool value = true) noexcept { return set(bit_error, value); }
  Set& set_signal(bool value = true) noexcept { return set(bit_signal, value); }
  Set& set_timer(bool value = true) noexcept { return set(bit_timer, value); }
  Set& set_event(bool value = true) noexcept { return set(bit_event, value); }

  // Constants for various interesting Set values.
  static constexpr Set no_bits() noexcept { return Set(0); }
  static constexpr Set all_bits() noexcept { return Set(~uint8_t(0)); }
  static constexpr Set readable_bit() noexcept { return Set(bit_readable); }
  static constexpr Set writable_bit() noexcept { return Set(bit_writable); }
  static constexpr Set priority_bit() noexcept { return Set(bit_priority); }
  static constexpr Set hangup_bit() noexcept { return Set(bit_hangup); }
  static constexpr Set error_bit() noexcept { return Set(bit_error); }
  static constexpr Set signal_bit() noexcept { return Set(bit_signal); }
  static constexpr Set timer_bit() noexcept { return Set(bit_timer); }
  static constexpr Set event_bit() noexcept { return Set(bit_event); }

  void append_to(std::string* out) const;
  std::string as_string() const;

 private:
  uint8_t bits_;
};

inline void swap(Set& a, Set& b) noexcept { a.swap(b); }
inline constexpr bool operator!=(const Set& a, const Set& b) noexcept {
  return !(a == b);
}
inline constexpr bool operator>(const Set& a, const Set& b) noexcept {
  return (b < a);
}
inline constexpr bool operator<=(const Set& a, const Set& b) noexcept {
  return !(b < a);
}
inline constexpr bool operator>=(const Set& a, const Set& b) noexcept {
  return !(a < b);
}

inline std::ostream& operator<<(std::ostream& os, Set x) {
  return (os << x.as_string());
}

#if 0
// The GNU ISO C++ Library doesn't implement std::is_trivially_copyable!
static_assert(std::is_trivially_copyable<Set>::value,
              "event::Set must be trivially copyable");
#endif

static_assert(std::is_standard_layout<Set>::value,
              "event::Set must be standard layout");

}  // namespace event

#endif  // EVENT_SET_H
