// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "file/file.h"

namespace file {

FileSystemPtr find(const std::string& fsname) {
  auto lock = base::acquire_lock(system_registry_mutex());
  return system_registry().find(fsname);
}

void statfs(event::Task* task, StatFS* out, const std::string& fsname,
            const std::string& path, const base::Options& opts) {
  auto fs = find(fsname);
  if (!fs) {
    if (task->start()) task->finish(base::Result::not_implemented());
    return;
  }
  fs->statfs(task, out, path, opts);
}

void stat(event::Task* task, Stat* out, const std::string& fsname,
          const std::string& path, const base::Options& opts) {
  auto fs = find(fsname);
  if (!fs) {
    if (task->start()) task->finish(base::Result::not_implemented());
    return;
  }
  fs->stat(task, out, path, opts);
}

void set_stat(event::Task* task, const std::string& fsname,
              const std::string& path, const SetStat& delta,
              const base::Options& opts) {
  auto fs = find(fsname);
  if (!fs) {
    if (task->start()) task->finish(base::Result::not_implemented());
    return;
  }
  fs->set_stat(task, path, delta, opts);
}

void open(event::Task* task, File* out, const std::string& fsname,
          const std::string& path, Mode mode, const base::Options& opts) {
  auto fs = find(fsname);
  if (!fs) {
    if (task->start()) task->finish(base::Result::not_implemented());
    return;
  }
  fs->open(task, out, path, mode, opts);
}

void link(event::Task* task, const std::string& fsname,
          const std::string& oldpath, const std::string& newpath,
          const base::Options& opts) {
  auto fs = find(fsname);
  if (!fs) {
    if (task->start()) task->finish(base::Result::not_implemented());
    return;
  }
  fs->link(task, oldpath, newpath, opts);
}

void symlink(event::Task* task, const std::string& fsname,
             const std::string& target, const std::string& linkpath,
             const base::Options& opts) {
  auto fs = find(fsname);
  if (!fs) {
    if (task->start()) task->finish(base::Result::not_implemented());
    return;
  }
  fs->symlink(task, target, linkpath, opts);
}

void unlink(event::Task* task, const std::string& fsname,
            const std::string& path, const base::Options& opts) {
  auto fs = find(fsname);
  if (!fs) {
    if (task->start()) task->finish(base::Result::not_implemented());
    return;
  }
  fs->unlink(task, path, opts);
}

void touch(event::Task* task, const std::string& fsname,
           const std::string& path, const base::Options& opts) {
  auto fs = find(fsname);
  if (!fs) {
    if (task->start()) task->finish(base::Result::not_implemented());
    return;
  }
  fs->touch(task, path, opts);
}

void opendir(event::Task* task, File* out, const std::string& fsname,
             const std::string& path, Mode mode, const base::Options& opts) {
  auto fs = find(fsname);
  if (!fs) {
    if (task->start()) task->finish(base::Result::not_implemented());
    return;
  }
  fs->opendir(task, out, path, mode, opts);
}

void mkdir(event::Task* task, const std::string& fsname,
           const std::string& path, const base::Options& opts) {
  auto fs = find(fsname);
  if (!fs) {
    if (task->start()) task->finish(base::Result::not_implemented());
    return;
  }
  fs->mkdir(task, path, opts);
}

void rmdir(event::Task* task, const std::string& fsname,
           const std::string& path, const base::Options& opts) {
  auto fs = find(fsname);
  if (!fs) {
    if (task->start()) task->finish(base::Result::not_implemented());
    return;
  }
  fs->rmdir(task, path, opts);
}

base::Result statfs(StatFS* out, const std::string& fsname,
                    const std::string& path, const base::Options& opts) {
  event::Task task;
  statfs(&task, out, fsname, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result stat(Stat* out, const std::string& fsname, const std::string& path,
                  const base::Options& opts) {
  event::Task task;
  stat(&task, out, fsname, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result set_stat(const std::string& fsname, const std::string& path,
                      const SetStat& delta, const base::Options& opts) {
  event::Task task;
  set_stat(&task, fsname, path, delta, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result open(File* out, const std::string& fsname, const std::string& path,
                  Mode mode, const base::Options& opts) {
  event::Task task;
  open(&task, out, fsname, path, mode, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result link(const std::string& fsname, const std::string& oldpath,
                  const std::string& newpath, const base::Options& opts) {
  event::Task task;
  link(&task, fsname, oldpath, newpath, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result symlink(const std::string& fsname, const std::string& target,
                     const std::string& linkpath, const base::Options& opts) {
  event::Task task;
  symlink(&task, fsname, target, linkpath, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result unlink(const std::string& fsname, const std::string& path,
                    const base::Options& opts) {
  event::Task task;
  unlink(&task, fsname, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result touch(const std::string& fsname, const std::string& path,
                   const base::Options& opts) {
  event::Task task;
  touch(&task, fsname, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result opendir(File* out, const std::string& fsname,
                     const std::string& path, Mode mode,
                     const base::Options& opts) {
  event::Task task;
  opendir(&task, out, fsname, path, mode, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result mkdir(const std::string& fsname, const std::string& path,
                   const base::Options& opts) {
  event::Task task;
  mkdir(&task, fsname, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result rmdir(const std::string& fsname, const std::string& path,
                   const base::Options& opts) {
  event::Task task;
  rmdir(&task, fsname, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

}  // namespace file
