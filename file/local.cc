// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "file/local.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>

#include <cerrno>

#include "base/cleanup.h"
#include "base/mutex.h"
#include "base/time/time.h"
#include "base/user.h"
#include "file/fd.h"
#include "file/registry.h"
#include "path/path.h"

namespace file {

namespace {

class LocalFS : public FileSystemImpl {
 public:
  LocalFS() noexcept : FileSystemImpl("local") {}

  void statfs(event::Task* task, StatFS* out, const std::string& path,
              const base::Options& opts) const override;

  void stat(event::Task* task, Stat* out, const std::string& path,
            const base::Options& opts) const override;

  void set_stat(event::Task* task, const std::string& path,
                const SetStat& delta, const base::Options& opts) override;

  void open(event::Task* task, File* out, const std::string& path, Mode mode,
            const base::Options& opts) override;

  void link(event::Task* task, const std::string& oldpath,
            const std::string& newpath, const base::Options& opts) override;

  void symlink(event::Task* task, const std::string& target,
               const std::string& linkpath, const base::Options& opts) override;

  void unlink(event::Task* task, const std::string& path,
              const base::Options& opts) override;

  static FileSystemPtr make() {
    auto ptr = std::make_shared<LocalFS>();
    ptr->set_self(ptr);
    return ptr;
  }
};

void LocalFS::statfs(event::Task* task, StatFS* out, const std::string& path,
                     const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = StatFS();

  struct statfs f;
  ::bzero(&f, sizeof(f));

  base::Result r;
  int rc = ::statfs(path.c_str(), &f);
  if (rc != 0) {
    int err_no = errno;
    r = base::Result::from_errno(err_no, "statfs(2)");
  } else {
    r = convert_statfs(out, f);
  }
  task->finish(std::move(r));
}

void LocalFS::stat(event::Task* task, Stat* out, const std::string& path,
                   const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = Stat();

  struct stat st;
  ::bzero(&st, sizeof(st));

  const file::Options& fo = opts;
  base::Result r;
  const char* what;
  int rc;
  if (fo.nofollow) {
    what = "lstat(2)";
    rc = ::lstat(path.c_str(), &st);
  } else {
    what = "stat(2)";
    rc = ::stat(path.c_str(), &st);
  }
  if (rc != 0) {
    int err_no = errno;
    r = base::Result::from_errno(err_no, what);
  } else {
    r = convert_stat(out, st);
  }
  task->finish(std::move(r));
}

void LocalFS::set_stat(event::Task* task, const std::string& path,
                       const SetStat& delta, const base::Options& opts) {
  if (!task->start()) return;

  bool has_owner, has_group, has_perm, has_mtime, has_atime;
  std::string owner, group;
  Perm perm;
  base::time::Time mtime, atime;

  std::tie(has_owner, owner) = delta.owner();
  std::tie(has_group, group) = delta.group();
  std::tie(has_perm, perm) = delta.perm();
  std::tie(has_mtime, mtime) = delta.mtime();
  std::tie(has_atime, atime) = delta.atime();

  const file::Options& fo = opts;
  int flags = 0;
  if (fo.nofollow) flags |= AT_SYMLINK_NOFOLLOW;

  if (has_mtime || has_atime) {
    struct timespec times[2];

    if (has_atime) {
      base::Result r = base::time::timespec_from_time(&times[0], atime);
      if (!r) {
        task->finish(std::move(r));
        return;
      }
    } else {
      times[0].tv_sec = 0;
      times[0].tv_nsec = UTIME_OMIT;
    }

    if (has_mtime) {
      base::Result r = base::time::timespec_from_time(&times[1], mtime);
      if (!r) {
        task->finish(std::move(r));
        return;
      }
    } else {
      times[1].tv_sec = 0;
      times[1].tv_nsec = UTIME_OMIT;
    }

    int rc = ::utimensat(AT_FDCWD, path.c_str(), times, flags);
    if (rc != 0) {
      int err_no = errno;
      task->finish(base::Result::from_errno(err_no, "utimensat(2)"));
      return;
    }
  }

  if (has_perm) {
    int rc = ::fchmodat(AT_FDCWD, path.c_str(), uint16_t(perm), flags);
    if (rc != 0) {
      int err_no = errno;
      task->finish(base::Result::from_errno(err_no, "fchmodat(2)"));
      return;
    }
  }

  if (has_owner || has_group) {
    uid_t uid = -1;
    gid_t gid = -1;

    if (has_owner) {
      base::User u;
      base::Result r = base::user_by_name(&u, owner);
      if (!r) {
        task->finish(std::move(r));
        return;
      }
      uid = u.uid;
    }

    if (has_group) {
      base::Group g;
      base::Result r = base::group_by_name(&g, group);
      if (!r) {
        task->finish(std::move(r));
        return;
      }
      gid = g.gid;
    }

    int rc = ::fchownat(AT_FDCWD, path.c_str(), uid, gid, flags);
    if (rc != 0) {
      int err_no = errno;
      task->finish(base::Result::from_errno(err_no, "fchownat(2)"));
      return;
    }
  }

  task->finish_ok();
}

void LocalFS::open(event::Task* task, File* out, const std::string& path,
                   Mode mode, const base::Options& opts) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = File();

  if (!mode.valid()) {
    task->finish(base::Result::invalid_argument("invalid mode"));
    return;
  }

  int flags;
  if (mode.read() && mode.write())
    flags = O_RDWR;
  else if (mode.write())
    flags = O_WRONLY;
  else if (mode.read())
    flags = O_RDONLY;
  else
    flags = O_RDONLY;
  if (mode.append()) flags |= O_APPEND;
  if (mode.create()) flags |= O_CREAT;
  if (mode.exclusive()) flags |= O_EXCL;
  if (mode.truncate()) flags |= O_TRUNC;
  const file::Options& fo = opts;
  if (fo.open_directory) flags |= O_DIRECTORY;
  if (fo.close_on_exec) flags |= O_CLOEXEC;
  if (fo.nonblocking_io) flags |= O_NONBLOCK;
  if (fo.direct_io) flags |= O_DIRECT;
  if (fo.nofollow) flags |= O_NOFOLLOW;
  if (fo.noatime) flags |= O_NOATIME;

  std::string cleaned = path::partial_clean(path);
  Perm perm = fo.masked_create_perm();
  if (fo.open_directory && mode.create()) {
    perm = fo.masked_create_dir_perm();
    int rc = ::mkdir(cleaned.c_str(), uint16_t(perm));
    if (rc != 0) {
      int err_no = errno;
      if (err_no != EEXIST || mode.exclusive()) {
        task->finish(base::Result::from_errno(err_no, "mkdir(2)"));
        return;
      }
    }
  }
  int fdnum = ::open(cleaned.c_str(), flags, uint16_t(perm));
  if (fdnum == -1) {
    int err_no = errno;
    task->finish(base::Result::from_errno(err_no, "open(2)"));
    return;
  }
  *out = fdfile(self(), cleaned, mode, base::wrapfd(fdnum));
  task->finish_ok();
}

void LocalFS::link(event::Task* task, const std::string& oldpath,
                   const std::string& newpath, const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  base::Result r;
  int rc = ::link(oldpath.c_str(), newpath.c_str());
  if (rc != 0) {
    int err_no = errno;
    r = base::Result::from_errno(err_no, "link(2)");
  }
  task->finish(std::move(r));
}

void LocalFS::symlink(event::Task* task, const std::string& target,
                      const std::string& linkpath, const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  base::Result r;
  int rc = ::symlink(target.c_str(), linkpath.c_str());
  if (rc != 0) {
    int err_no = errno;
    r = base::Result::from_errno(err_no, "symlink(2)");
  }
  task->finish(std::move(r));
}

void LocalFS::unlink(event::Task* task, const std::string& path,
                     const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  int flags = 0;
  const file::Options& fo = opts;
  if (fo.remove_directory) flags |= AT_REMOVEDIR;

  base::Result r;
  int rc = ::unlinkat(AT_FDCWD, path.c_str(), flags);
  if (rc != 0) {
    int err_no = errno;
    r = base::Result::from_errno(err_no, "unlinkat(2)");
  }
  task->finish(std::move(r));
}

static std::mutex g_mu;
static FileSystemPtr* g_fs = nullptr;  // protected by g_mu

}  // anonymous namespace

FileSystemPtr local_filesystem() {
  auto lock = base::acquire_lock(g_mu);
  if (!g_fs) g_fs = new FileSystemPtr;
  if (!*g_fs) *g_fs = LocalFS::make();
  return *g_fs;
}

}  // namespace file

static void init() __attribute__((constructor));
static void init() {
  auto lock = base::acquire_lock(file::system_registry_mutex());
  file::system_registry_mutable().add(nullptr, 50, file::local_filesystem());
}
