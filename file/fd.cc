// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "file/fd.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>

#include "base/cleanup.h"
#include "base/time/time.h"
#include "base/user.h"

namespace file {

namespace {

static int system_whence(Whence whence) {
  switch (whence) {
    case Whence::start:
      return SEEK_SET;
    case Whence::current:
      return SEEK_CUR;
    case Whence::end:
      return SEEK_END;
  }
  LOG(DFATAL) << "invalid file::Whence";
  return SEEK_CUR;
}

static FileType filetype_from_mode(mode_t mode) noexcept {
  if (S_ISREG(mode)) return FileType::regular;
  if (S_ISDIR(mode)) return FileType::directory;
  if (S_ISCHR(mode)) return FileType::char_device;
  if (S_ISBLK(mode)) return FileType::block_device;
  if (S_ISFIFO(mode)) return FileType::fifo;
  if (S_ISSOCK(mode)) return FileType::socket;
  if (S_ISLNK(mode)) return FileType::symbolic_link;
  return FileType::unknown;
}

static FileType filetype_from_dtype(unsigned char dt) noexcept {
  switch (dt) {
    case DT_REG:
      return FileType::regular;
    case DT_DIR:
      return FileType::directory;
    case DT_CHR:
      return FileType::char_device;
    case DT_BLK:
      return FileType::block_device;
    case DT_FIFO:
      return FileType::fifo;
    case DT_SOCK:
      return FileType::socket;
    case DT_LNK:
      return FileType::symbolic_link;
  }
  return FileType::unknown;
}

class FDFile : public FileImpl {
 public:
  FDFile(FileSystemPtr fs, std::string path, Mode mode, base::FD fd) noexcept
      : FileImpl(std::move(fs), std::move(path), mode),
        fd_(std::move(fd)),
        r_(io::fdreader(fd_)),
        w_(io::fdwriter(fd_)) {}

  io::Reader reader() override { return r_; }
  io::Writer writer() override { return w_; }

  void readdir(event::Task* task, std::vector<DirEntry>* out,
               const base::Options& opts) const override;

  void statfs(event::Task* task, StatFS* out,
              const base::Options& opts) const override;

  void stat(event::Task* task, Stat* out,
            const base::Options& opts) const override;

  void size(event::Task* task, int64_t* out,
            const base::Options& opts) const override;

  void tell(event::Task* task, int64_t* out,
            const base::Options& opts) const override;

  void set_stat(event::Task* task, const SetStat& delta,
                const base::Options& opts) override;

  void seek(event::Task* task, int64_t off, Whence whence,
            const base::Options& opts) override;

  void truncate_at(event::Task* task, int64_t off,
                   const base::Options& opts) override;

  void close(event::Task* task, const base::Options& opts) override;

 private:
  base::FD fd_;
  io::Reader r_;
  io::Writer w_;
};

void FDFile::readdir(event::Task* task, std::vector<DirEntry>* out,
                     const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  out->clear();

  std::vector<base::DEntry> tmp;
  auto r = base::readdir_all(&tmp, fd_, path().c_str());
  if (r) {
    for (auto& dent : tmp) {
      unsigned char dtype = std::get<1>(dent);
      std::string& name = std::get<2>(dent);
      out->emplace_back(std::move(name), filetype_from_dtype(dtype));
    }
  }
  task->finish(std::move(r));
}

void FDFile::statfs(event::Task* task, StatFS* out,
                    const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = StatFS();

  struct statfs f;
  ::bzero(&f, sizeof(f));

  base::Result r;
  auto fdpair = fd_->acquire_fd();
  int rc = ::fstatfs(fdpair.first, &f);
  if (rc != 0) {
    int err_no = errno;
    r = base::Result::from_errno(err_no, "statfs(2)");
  } else {
    r = convert_statfs(out, f);
  }
  task->finish(std::move(r));
}

void FDFile::stat(event::Task* task, Stat* out,
                  const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  *out = Stat();

  base::Result r;
  struct stat st;
  ::bzero(&st, sizeof(st));
  auto fdpair = fd_->acquire_fd();
  int rc = ::fstat(fdpair.first, &st);
  if (rc != 0) {
    int err_no = errno;
    r = base::Result::from_errno(err_no, "fstat(2)");
  } else {
    r = convert_stat(out, st);
  }
  task->finish(std::move(r));
}

void FDFile::size(event::Task* task, int64_t* out,
                  const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;

  base::Result r;
  struct stat st;
  ::bzero(&st, sizeof(st));
  auto fdpair = fd_->acquire_fd();
  int rc = ::fstat(fdpair.first, &st);
  if (rc != 0) {
    int err_no = errno;
    r = base::Result::from_errno(err_no, "fstat(2)");
    st.st_size = -1;
  }
  *out = st.st_size;
  task->finish(std::move(r));
}

void FDFile::tell(event::Task* task, int64_t* out,
                  const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;

  off_t tmp = -1;
  base::Result r = base::seek(&tmp, fd_, 0, SEEK_CUR);
  *out = tmp;
  task->finish(std::move(r));
}

void FDFile::set_stat(event::Task* task, const SetStat& delta,
                      const base::Options& opts) {
  CHECK_NOTNULL(task);
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

  auto fdpair = fd_->acquire_fd();

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

    int rc = ::futimens(fdpair.first, times);
    if (rc != 0) {
      int err_no = errno;
      task->finish(base::Result::from_errno(err_no, "futimens(2)"));
      return;
    }
  }

  if (has_perm) {
    int rc = ::fchmod(fdpair.first, uint16_t(perm));
    if (rc != 0) {
      int err_no = errno;
      task->finish(base::Result::from_errno(err_no, "fchmod(2)"));
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

    int rc = ::fchown(fdpair.first, uid, gid);
    if (rc != 0) {
      int err_no = errno;
      task->finish(base::Result::from_errno(err_no, "fchown(2)"));
      return;
    }
  }

  task->finish_ok();
}

void FDFile::seek(event::Task* task, int64_t off, Whence whence,
                  const base::Options& opts) {
  CHECK_NOTNULL(task);
  base::Result r = base::seek(nullptr, fd_, off, system_whence(whence));
  if (task->start()) task->finish(std::move(r));
}

void FDFile::truncate_at(event::Task* task, int64_t off,
                         const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  base::Result r;
  auto fdpair = fd_->acquire_fd();
  int rc = ::ftruncate(fdpair.first, off);
  if (rc != 0) {
    int err_no = errno;
    r = base::Result::from_errno(err_no, "ftruncate(2)");
  }
  task->finish(std::move(r));
}

void FDFile::close(event::Task* task, const base::Options& opts) {
  CHECK_NOTNULL(task);

  auto r = fd_->close();
  if (task->start()) task->finish(std::move(r));
}

}  // anonymous namespace

base::Result convert_statfs(StatFS* out, const struct statfs& f) {
  StatFS tmp;
  tmp.optimal_block_size = f.f_bsize;
  tmp.used_blocks = f.f_blocks;
  tmp.free_blocks = f.f_bfree;
  tmp.used_inodes = f.f_files;
  tmp.free_inodes = f.f_ffree;
  *out = tmp;
  return base::Result();
}

base::Result convert_stat(Stat* out, const struct stat& st) {
  base::User u;
  base::Group g;
  base::time::Time ctime, mtime, atime;
  base::Result r =
      base::user_by_id(&u, st.st_uid)
          .and_then(base::group_by_id(&g, st.st_gid))
          .and_then(base::time::time_from_timespec(&ctime, &st.st_ctim))
          .and_then(base::time::time_from_timespec(&mtime, &st.st_mtim))
          .and_then(base::time::time_from_timespec(&atime, &st.st_atim));
  if (!r) return r;
  Stat tmp;
  tmp.type = filetype_from_mode(st.st_mode);
  tmp.perm = st.st_mode;
  tmp.owner = std::move(u.name);
  tmp.group = std::move(g.name);
  tmp.link_count = st.st_nlink;
  tmp.size = st.st_size;
  tmp.size_blocks = st.st_blocks;
  tmp.optimal_block_size = st.st_blksize;
  tmp.change_time = ctime;
  tmp.modify_time = mtime;
  tmp.access_time = atime;
  *out = tmp;
  return base::Result();
}

File fdfile(FileSystemPtr fs, std::string path, Mode mode, base::FD fd) {
  return File(std::make_shared<FDFile>(std::move(fs), std::move(path), mode,
                                       std::move(fd)));
}

}  // namespace file
