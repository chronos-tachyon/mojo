// file/options.h - Knobs for file::* behavior
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef FILE_OPTIONS_H
#define FILE_OPTIONS_H

#include <string>

#include "base/options.h"
#include "file/perm.h"

namespace file {

struct Options : public base::OptionsType {
  std::string user;
  std::string group;
  Perm create_perm;
  Perm create_dir_perm;
  Perm perm_mask;
  bool open_directory;
  bool remove_directory;
  bool close_on_exec;
  bool nonblocking_io;
  bool direct_io;
  bool nofollow;
  bool noatime;

  // Options is default constructible.
  Options() noexcept : create_perm(0666),
                       create_dir_perm(0777),
                       perm_mask(022),
                       open_directory(false),
                       remove_directory(false),
                       close_on_exec(true),
                       nonblocking_io(true),
                       direct_io(false),
                       nofollow(false),
                       noatime(false) {}

  // Options is copyable and moveable.
  Options(const Options&) = default;
  Options(Options&&) noexcept = default;
  Options& operator=(const Options&) = default;
  Options& operator=(Options&&) noexcept = default;

  // Resets this file::Options to the default values.
  void reset() { *this = Options(); }

  Perm masked_create_perm() const noexcept {
    return create_perm & ~perm_mask;
  }

  Perm masked_create_dir_perm() const noexcept {
    return create_dir_perm & ~perm_mask;
  }
};

}  // namespace file

#endif  // FILE_OPTIONS_H
