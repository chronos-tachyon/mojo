// base/user.h - Users and groups
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_USER_H
#define BASE_USER_H

#include <cstdint>
#include <string>
#include <vector>

#include "base/result.h"

namespace base {

// User holds metadata about a user, typically one on the local system.
struct User {
  int32_t uid;
  int32_t gid;
  std::string name;
  std::string gecos;
  std::string homedir;
  std::string shell;

  User(int32_t uid = -1, int32_t gid = -1, std::string name = std::string(),
       std::string gecos = std::string(), std::string homedir = std::string(),
       std::string shell = std::string()) noexcept
      : uid(uid),
        gid(gid),
        name(std::move(name)),
        gecos(std::move(gecos)),
        homedir(std::move(homedir)),
        shell(std::move(shell)) {}

  User(const User&) = default;
  User& operator=(const User&) = default;

  User(User&& other) noexcept : uid(other.uid),
                                gid(other.gid),
                                name(std::move(other.name)),
                                gecos(std::move(other.gecos)),
                                homedir(std::move(other.homedir)),
                                shell(std::move(other.shell)) {
    other.uid = other.gid = -1;
  }
  User& operator=(User&& other) noexcept {
    reset();
    swap(other);
    return *this;
  }

  void reset() noexcept {
    uid = -1;
    gid = -1;
    name.clear();
    gecos.clear();
    homedir.clear();
    shell.clear();
  }

  void swap(User& other) noexcept {
    using std::swap;
    swap(uid, other.uid);
    swap(gid, other.gid);
    swap(name, other.name);
    swap(gecos, other.gecos);
    swap(homedir, other.homedir);
    swap(shell, other.shell);
  }

  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept;
  std::string as_string() const;
};

inline void swap(User& a, User& b) noexcept { a.swap(b); }

bool operator==(const User& a, const User& b) noexcept;
inline bool operator!=(const User& a, const User& b) noexcept {
  return !(a == b);
}

// Group holds metadata about a group, typically one on the local system.
struct Group {
  int32_t gid;
  std::string name;
  std::vector<std::string> members;

  Group(int32_t gid = -1, std::string name = std::string(),
        std::vector<std::string> members = std::vector<std::string>()) noexcept
      : gid(gid),
        name(std::move(name)),
        members(std::move(members)) {}

  Group(const Group&) = default;
  Group& operator=(const Group&) = default;

  Group(Group&& other) noexcept : gid(other.gid),
                                  name(std::move(other.name)),
                                  members(std::move(other.members)) {
    other.gid = -1;
  }
  Group& operator=(Group&& other) noexcept {
    reset();
    swap(other);
    return *this;
  }

  void reset() noexcept {
    gid = -1;
    name.clear();
    members.clear();
  }

  void swap(Group& other) noexcept {
    using std::swap;
    swap(gid, other.gid);
    swap(name, other.name);
    swap(members, other.members);
  }

  void append_to(std::string* out) const;
  std::size_t length_hint() const noexcept;
  std::string as_string() const;
};

inline void swap(Group& a, Group& b) noexcept { a.swap(b); }

bool operator==(const Group& a, const Group& b) noexcept;
inline bool operator!=(const Group& a, const Group& b) noexcept {
  return !(a == b);
}

// Retrieves information about the specified account, looked up via the
// provided field.
base::Result user_by_id(User* out, int32_t id);
base::Result user_by_name(User* out, const std::string& name);
base::Result group_by_id(Group* out, int32_t id);
base::Result group_by_name(Group* out, const std::string& name);

}  // namespace base

#endif  // BASE_USER_H
