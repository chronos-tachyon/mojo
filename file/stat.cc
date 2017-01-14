// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "file/stat.h"

#include "base/concat.h"

template <typename T>
static std::size_t LH(T arg) noexcept {
  using base::length_hint;
  return length_hint(arg);
}

namespace file {

static const char* const kFileTypeNames[] = {
    "unknown",      "regular", "directory", "char_device",
    "block_device", "fifo",    "socket",    "symbolic_link",
};

const char* filetype_name(FileType type) noexcept {
  return kFileTypeNames[static_cast<uint8_t>(type)];
}

void append_to(std::string* out, FileType type) {
  out->append(filetype_name(type));
}

std::size_t length_hint(FileType) noexcept { return 13; }

bool operator==(const DirEntry& a, const DirEntry& b) {
  return a.name == b.name && a.type == b.type;
}

bool operator<(const DirEntry& a, const DirEntry& b) {
  if (a.name != b.name) return a.name < b.name;
  return a.type < b.type;
}

void Stat::append_to(std::string* out) const {
  base::concat_to(out, "Stat{type=", type, ", perm=", perm, ", owner=\"", owner,
                  "\", group=\"", group, "\", link_count=", link_count,
                  ", size=", size, ", size_blocks=", size_blocks,
                  ", optimal_block_size=", optimal_block_size, "}");
}

std::size_t Stat::length_hint() const noexcept {
  return 92 + LH(type) + perm.length_hint() + LH(owner) + LH(group) +
         LH(link_count) + LH(size) + LH(size_blocks) + LH(optimal_block_size);
}

std::string Stat::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

void StatFS::append_to(std::string* out) const {
  base::concat_to(out, "StatFS{optimal_block_size=", optimal_block_size,
                  ", used_blocks=", used_blocks, ", free_blocks=", free_blocks,
                  ", used_inodes=", used_inodes, ", free_inodes=", free_inodes,
                  "}");
}

std::size_t StatFS::length_hint() const noexcept {
  return 83 + LH(optimal_block_size) + LH(used_blocks) + LH(free_blocks) +
         LH(used_inodes) + LH(free_inodes);
}

std::string StatFS::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

}  // namespace file
