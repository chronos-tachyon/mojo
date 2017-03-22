// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/user.h"

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "base/backport.h"
#include "base/concat.h"
#include "base/logging.h"

template <typename T>
static std::size_t LH(T arg) {
  using base::length_hint;
  return length_hint(arg);
}

namespace base {

void User::append_to(std::string* out) const {
  base::concat_to(out, name, "(", uid, ")");
}

std::size_t User::length_hint() const noexcept {
  return 2 + name.size() + LH(uid);
}

std::string User::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

bool operator==(const User& a, const User& b) noexcept {
  return a.uid == b.uid && a.gid == b.gid && a.name == b.name &&
         a.gecos == b.gecos && a.homedir == b.homedir && a.shell == b.shell;
}

void Group::append_to(std::string* out) const {
  base::concat_to(out, name, "(", gid, ")");
}

std::size_t Group::length_hint() const noexcept {
  return 2 + name.size() + LH(gid);
}

std::string Group::as_string() const {
  std::string out;
  append_to(&out);
  return out;
}

bool operator==(const Group& a, const Group& b) noexcept {
  if (a.gid != b.gid || a.name != b.name ||
      a.members.size() != b.members.size())
    return false;
  for (std::size_t i = 0, n = a.members.size(); i < n; ++i) {
    if (a.members[i] != b.members[i]) return false;
  }
  return true;
}

template <typename Functor>
static base::Result user_common(User* out, const char* what, Functor func) {
  std::vector<char> buf;
  struct passwd pw;
  struct passwd* ptr;
  long len;
  int rc;

  *out = User();

  len = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (len < 1024) len = 1024;
  buf.resize(len);

redo:
  ::bzero(&pw, sizeof(pw));
  ptr = nullptr;
  rc = func(&pw, buf.data(), buf.size(), &ptr);
  if (ptr == nullptr) {
    if (rc == 0) rc = ENOENT;
    if (rc == EINTR) goto redo;
    if (rc == ERANGE) {
      buf.resize(buf.size() * 2);
      goto redo;
    }
    return base::Result::from_errno(rc, what);
  }
  *out = User(pw.pw_uid, pw.pw_gid, pw.pw_name, pw.pw_gecos, pw.pw_dir,
              pw.pw_shell);
  return base::Result();
}

template <typename Functor>
static base::Result group_common(Group* out, const char* what, Functor func) {
  std::vector<char> buf;
  struct group gr;
  struct group* ptr;
  long len;
  int rc;

  *out = Group();

  len = sysconf(_SC_GETGR_R_SIZE_MAX);
  if (len < 1024) len = 1024;
  buf.resize(len);

redo:
  ::bzero(&gr, sizeof(gr));
  ptr = nullptr;
  rc = func(&gr, buf.data(), buf.size(), &ptr);
  if (ptr == nullptr) {
    if (rc == 0) rc = ENOENT;
    if (rc == EINTR) goto redo;
    if (rc == ERANGE) {
      buf.resize(buf.size() * 2);
      goto redo;
    }
    return base::Result::from_errno(rc, "getpwuid_r(3)");
  }
  std::vector<std::string> members;
  for (char** ptr = gr.gr_mem; *ptr; ++ptr) {
    members.push_back(*ptr);
  }
  *out = Group(gr.gr_gid, gr.gr_name, std::move(members));
  return base::Result();
}

base::Result user_by_id(User* out, int32_t id) {
  return user_common(
      out, "getpwuid_r(3)",
      [id](struct passwd* pw, char* buf, std::size_t len, struct passwd** ptr) {
        return ::getpwuid_r(id, pw, buf, len, ptr);
      });
}

base::Result user_by_name(User* out, const std::string& name) {
  return user_common(out, "getpwnam_r(3)",
                     [&name](struct passwd* pw, char* buf, std::size_t len,
                             struct passwd** ptr) {
                       return ::getpwnam_r(name.c_str(), pw, buf, len, ptr);
                     });
}

base::Result group_by_id(Group* out, int32_t id) {
  return group_common(
      out, "getgrgid_r(3)",
      [id](struct group* gr, char* buf, std::size_t len, struct group** ptr) {
        return ::getgrgid_r(id, gr, buf, len, ptr);
      });
}

base::Result group_by_name(Group* out, const std::string& name) {
  return group_common(out, "getgrnam_r(3)",
                      [&name](struct group* gr, char* buf, std::size_t len,
                              struct group** ptr) {
                        return ::getgrnam_r(name.c_str(), gr, buf, len, ptr);
                      });
}

static std::unique_ptr<User> must_user(int32_t id) {
  auto ptr = backport::make_unique<User>();
  CHECK_OK(user_by_id(ptr.get(), id));
  return ptr;
}

const User& real_user() {
  static const User& ref = *must_user(::getuid()).release();
  return ref;
}

const User& effective_user() {
  static const User& ref = *must_user(::geteuid()).release();
  return ref;
}

static std::unique_ptr<Group> must_group(int32_t id) {
  auto ptr = backport::make_unique<Group>();
  CHECK_OK(group_by_id(ptr.get(), id));
  return ptr;
}

const Group& real_group() {
  static const Group& ref = *must_group(::getgid()).release();
  return ref;
}

const Group& effective_group() {
  static const Group& ref = *must_group(::getegid()).release();
  return ref;
}

}  // namespace base
