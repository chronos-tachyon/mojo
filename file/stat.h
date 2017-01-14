// file/stat.h - File and filesystem stat data
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef FILE_STAT_H
#define FILE_STAT_H

#include <ostream>

#include "base/time.h"
#include "file/perm.h"

namespace file {

enum class FileType : uint8_t {
  unknown = 0,
  regular = 1,
  directory = 2,
  char_device = 3,
  block_device = 4,
  fifo = 5,
  socket = 6,
  symbolic_link = 7,
};

const char* filetype_name(FileType type) noexcept;
void append_to(std::string* out, FileType type);
std::size_t length_hint(FileType type) noexcept;

inline std::ostream& operator<<(std::ostream& os, FileType type) {
  std::string out;
  append_to(&out, type);
  return (os << out);
}

struct DirEntry {
  std::string name;
  FileType type;

  DirEntry() : type(FileType::unknown) {}
  DirEntry(std::string name, FileType type) noexcept : name(std::move(name)),
                                                       type(type) {}
};

bool operator==(const DirEntry& a, const DirEntry& b);
bool operator<(const DirEntry& a, const DirEntry& b);

struct StatFS {
  std::size_t optimal_block_size;
  std::size_t used_blocks;
  std::size_t free_blocks;
  std::size_t used_inodes;
  std::size_t free_inodes;

  std::size_t total_blocks() const noexcept {
    return used_blocks + free_blocks;
  }

  std::size_t total_inodes() const noexcept {
    return used_inodes + free_inodes;
  }

  StatFS() noexcept : optimal_block_size(0),
                      used_blocks(0),
                      free_blocks(0),
                      used_inodes(0),
                      free_inodes(0) {}
  StatFS(const StatFS&) = default;
  StatFS(StatFS&&) noexcept = default;
  StatFS& operator=(const StatFS&) = default;
  StatFS& operator=(StatFS&&) noexcept = default;
  void clear() noexcept { *this = StatFS(); }
  void swap(StatFS& other) noexcept { std::swap(*this, other); }
  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept;
  std::string as_string() const;
};

inline std::ostream& operator<<(std::ostream& os, const StatFS& statfs) {
  return (os << statfs.as_string());
}

struct Stat {
  FileType type;
  Perm perm;
  std::string owner;
  std::string group;
  std::size_t link_count;
  std::size_t size;
  std::size_t size_blocks;  // 512-byte blocks
  std::size_t optimal_block_size;
  base::Time create_time;
  base::Time change_time;
  base::Time modify_time;
  base::Time access_time;

  Stat() noexcept : type(FileType::unknown),
                    link_count(0),
                    size(0),
                    size_blocks(0),
                    optimal_block_size(0) {}
  Stat(const Stat&) = default;
  Stat(Stat&&) noexcept = default;
  Stat& operator=(const Stat&) = default;
  Stat& operator=(Stat&&) noexcept = default;
  void clear() noexcept { *this = Stat(); }
  void swap(Stat& other) noexcept { std::swap(*this, other); }
  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept;
  std::string as_string() const;
};

inline std::ostream& operator<<(std::ostream& os, const Stat& stat) {
  return (os << stat.as_string());
}

class SetStat {
 private:
  enum {
    bit_owner = (1U << 0),
    bit_group = (1U << 1),
    bit_perm = (1U << 2),
    bit_mtime = (1U << 3),
    bit_atime = (1U << 4),
  };

  bool has(uint8_t mask) const noexcept { return (has_ & mask) != 0; }

 public:
  SetStat() noexcept : has_(0) {}

  SetStat(const SetStat&) = default;
  SetStat& operator=(const SetStat&) = default;

  SetStat(SetStat&& other) noexcept : owner_(std::move(other.owner_)),
                                      group_(std::move(other.group_)),
                                      perm_(other.perm_),
                                      mtime_(other.mtime_),
                                      atime_(other.atime_),
                                      has_(other.has_) {
    other.reset();
  }
  SetStat& operator=(SetStat&& other) noexcept {
    owner_ = std::move(other.owner_);
    group_ = std::move(other.group_);
    perm_ = other.perm_;
    mtime_ = other.mtime_;
    atime_ = other.atime_;
    has_ = other.has_;
    other.reset();
    return *this;
  }

  void reset() noexcept { *this = SetStat(); }

  void swap(SetStat& other) noexcept {
    using std::swap;
    swap(owner_, other.owner_);
    swap(group_, other.group_);
    swap(perm_, other.perm_);
    swap(mtime_, other.mtime_);
    swap(atime_, other.atime_);
    swap(has_, other.has_);
  }

  std::pair<bool, std::string> owner() const {
    return std::make_pair(has(bit_owner), owner_);
  }
  void reset_owner() noexcept {
    has_ &= ~bit_owner;
    owner_.clear();
  }
  void set_owner(std::string owner) noexcept {
    owner_ = std::move(owner);
    has_ |= bit_owner;
  }

  std::pair<bool, std::string> group() const {
    return std::make_pair(has(bit_group), group_);
  }
  void reset_group() noexcept {
    has_ &= ~bit_group;
    group_.clear();
  }
  void set_group(std::string group) noexcept {
    group_ = std::move(group);
    has_ |= bit_group;
  }

  std::pair<bool, Perm> perm() const noexcept {
    return std::make_pair(has(bit_perm), perm_);
  }
  void reset_perm() noexcept {
    has_ &= ~bit_perm;
    perm_ = 0;
  }
  void set_perm(Perm perm) noexcept {
    perm_ = perm;
    has_ |= bit_perm;
  }

  std::pair<bool, base::Time> mtime() const {
    return std::make_pair(has(bit_mtime), mtime_);
  }
  void reset_mtime() noexcept {
    has_ &= ~bit_mtime;
    mtime_ = base::Time();
  }
  void set_mtime(base::Time mtime) noexcept {
    mtime_ = std::move(mtime);
    has_ |= bit_mtime;
  }

  std::pair<bool, base::Time> atime() const {
    return std::make_pair(has(bit_atime), atime_);
  }
  void reset_atime() noexcept {
    has_ &= ~bit_atime;
    atime_ = base::Time();
  }
  void set_atime(base::Time atime) noexcept {
    atime_ = std::move(atime);
    has_ |= bit_atime;
  }

 private:
  std::string owner_;
  std::string group_;
  Perm perm_;
  base::Time mtime_;
  base::Time atime_;
  uint8_t has_;
};

}  // namespace file

#endif  // FILE_STAT_H
