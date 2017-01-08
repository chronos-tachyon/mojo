// file/perm.h - Unix permissions
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef FILE_PERM_H
#define FILE_PERM_H

#include <cstdint>
#include <ostream>
#include <string>

namespace file {

class UserPerm {
 public:
  static constexpr UserPerm s_bit() noexcept { return UserPerm(8); }
  static constexpr UserPerm r_bit() noexcept { return UserPerm(4); }
  static constexpr UserPerm w_bit() noexcept { return UserPerm(2); }
  static constexpr UserPerm x_bit() noexcept { return UserPerm(1); }

  constexpr UserPerm() noexcept : bits_(0) {}
  constexpr UserPerm(uint8_t bits) noexcept : bits_(bits & 15) {}
  constexpr UserPerm(const UserPerm&) noexcept = default;
  constexpr UserPerm(UserPerm&&) noexcept = default;
  UserPerm& operator=(const UserPerm&) noexcept = default;
  UserPerm& operator=(UserPerm&&) noexcept = default;

  UserPerm& operator|=(UserPerm b) noexcept {
    bits_ |= b.bits_;
    return *this;
  }
  UserPerm& operator&=(UserPerm b) noexcept {
    bits_ &= b.bits_;
    return *this;
  }
  UserPerm& operator^=(UserPerm b) noexcept {
    bits_ ^= b.bits_;
    return *this;
  }

  friend constexpr UserPerm operator~(UserPerm a) noexcept {
    return UserPerm(~a.bits_);
  }
  friend constexpr UserPerm operator|(UserPerm a, UserPerm b) noexcept {
    return UserPerm(a.bits_ | b.bits_);
  }
  friend constexpr UserPerm operator&(UserPerm a, UserPerm b) noexcept {
    return UserPerm(a.bits_ & b.bits_);
  }
  friend constexpr UserPerm operator^(UserPerm a, UserPerm b) noexcept {
    return UserPerm(a.bits_ ^ b.bits_);
  }

  constexpr explicit operator bool() const noexcept { return bits_ != 0; }
  constexpr explicit operator uint8_t() const noexcept { return bits_; }

  constexpr bool has(UserPerm mask) const noexcept {
    return (bits_ & mask.bits_) != 0;
  }

  constexpr bool setxid() const noexcept { return has(s_bit()); }
  constexpr bool read() const noexcept { return has(r_bit()); }
  constexpr bool write() const noexcept { return has(w_bit()); }
  constexpr bool exec() const noexcept { return has(x_bit()); }

  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept { return 4; }
  std::string as_string() const;

 private:
  uint8_t bits_;
};

inline std::ostream& operator<<(std::ostream& os, UserPerm u) {
  return (os << '"' << u.as_string() << '"');
}

class Perm {
 public:
  static constexpr Perm us_bit() noexcept { return Perm(04000); }
  static constexpr Perm gs_bit() noexcept { return Perm(02000); }
  static constexpr Perm t_bit() noexcept { return Perm(01000); }
  static constexpr Perm ur_bit() noexcept { return Perm(0400); }
  static constexpr Perm uw_bit() noexcept { return Perm(0200); }
  static constexpr Perm ux_bit() noexcept { return Perm(0100); }
  static constexpr Perm gr_bit() noexcept { return Perm(0040); }
  static constexpr Perm gw_bit() noexcept { return Perm(0020); }
  static constexpr Perm gx_bit() noexcept { return Perm(0010); }
  static constexpr Perm or_bit() noexcept { return Perm(0004); }
  static constexpr Perm ow_bit() noexcept { return Perm(0002); }
  static constexpr Perm ox_bit() noexcept { return Perm(0001); }

  static constexpr Perm s_mask() noexcept { return Perm(06000); }
  static constexpr Perm r_mask() noexcept { return Perm(0444); }
  static constexpr Perm w_mask() noexcept { return Perm(0222); }
  static constexpr Perm x_mask() noexcept { return Perm(0111); }
  static constexpr Perm u_mask() noexcept { return Perm(04700); }
  static constexpr Perm g_mask() noexcept { return Perm(02070); }
  static constexpr Perm o_mask() noexcept { return Perm(00007); }

  constexpr Perm() noexcept : bits_(0) {}
  constexpr Perm(uint16_t bits) noexcept : bits_(bits & 07777) {}
  constexpr Perm(const Perm&) noexcept = default;
  constexpr Perm(Perm&&) noexcept = default;
  Perm& operator=(const Perm&) noexcept = default;
  Perm& operator=(Perm&&) noexcept = default;

  Perm& operator|=(Perm b) noexcept {
    bits_ |= b.bits_;
    return *this;
  }
  Perm& operator&=(Perm b) noexcept {
    bits_ &= b.bits_;
    return *this;
  }
  Perm& operator^=(Perm b) noexcept {
    bits_ ^= b.bits_;
    return *this;
  }

  friend constexpr Perm operator~(Perm a) noexcept { return Perm(~a.bits_); }
  friend constexpr Perm operator|(Perm a, Perm b) noexcept {
    return Perm(a.bits_ | b.bits_);
  }
  friend constexpr Perm operator&(Perm a, Perm b) noexcept {
    return Perm(a.bits_ & b.bits_);
  }
  friend constexpr Perm operator^(Perm a, Perm b) noexcept {
    return Perm(a.bits_ ^ b.bits_);
  }

  constexpr explicit operator bool() const noexcept { return bits_ != 0; }
  constexpr explicit operator uint16_t() const noexcept { return bits_; }

  constexpr bool has(Perm mask) const noexcept {
    return (bits_ & mask.bits_) != 0;
  }

  constexpr bool setuid() const noexcept { return has(us_bit()); }
  constexpr bool setgid() const noexcept { return has(gs_bit()); }
  constexpr bool sticky() const noexcept { return has(t_bit()); }
  constexpr bool user_read() const noexcept { return has(ur_bit()); }
  constexpr bool user_write() const noexcept { return has(uw_bit()); }
  constexpr bool user_exec() const noexcept { return has(ux_bit()); }
  constexpr bool group_read() const noexcept { return has(gr_bit()); }
  constexpr bool group_write() const noexcept { return has(gw_bit()); }
  constexpr bool group_exec() const noexcept { return has(gx_bit()); }
  constexpr bool other_read() const noexcept { return has(or_bit()); }
  constexpr bool other_write() const noexcept { return has(ow_bit()); }
  constexpr bool other_exec() const noexcept { return has(ox_bit()); }

  constexpr bool setxid() const noexcept { return has(s_mask()); }
  constexpr bool read() const noexcept { return has(r_mask()); }
  constexpr bool write() const noexcept { return has(w_mask()); }
  constexpr bool exec() const noexcept { return has(x_mask()); }

  constexpr UserPerm user() const noexcept {
    return UserPerm(((bits_ >> 6) & 7) | ((bits_ >> 8) & 8));
  }
  constexpr UserPerm group() const noexcept {
    return UserPerm(((bits_ >> 3) & 7) | ((bits_ >> 7) & 8));
  }
  constexpr UserPerm other() const noexcept { return UserPerm(bits_ & 7); }

  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept { return 5; }
  std::string as_string() const;

 private:
  uint16_t bits_;
};

inline std::ostream& operator<<(std::ostream& os, Perm p) {
  return (os << p.as_string());
}

}  // namespace file

#endif  // FILE_PERM_H
