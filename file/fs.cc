// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "file/fs.h"

#include <algorithm>
#include <stdexcept>

#include "base/logging.h"

namespace file {

namespace {
struct OpenCloseHelper {
  event::Task* const task;
  base::Options options;
  event::Task subtask;
  File file;

  OpenCloseHelper(event::Task* t, const base::Options& o) noexcept
      : task(t),
        options(o) {
    task->add_subtask(&subtask);
  }

  base::Result open_complete() {
    base::Result r = subtask.result();
    if (!r) {
      task->finish(std::move(r));
      delete this;
      return base::Result();
    }
    subtask.reset();
    task->add_subtask(&subtask);
    file.close(&subtask, options);
    subtask.on_finished(event::callback([this] { return close_complete(); }));
    return base::Result();
  }

  base::Result close_complete() {
    task->finish(subtask.result());
    delete this;
    return base::Result();
  }
};
}  // anonymous namespace

void FileSystemImpl::touch(event::Task* task, const std::string& path,
                           const base::Options& opts) {
  if (!task->start()) return;
  auto* h = new OpenCloseHelper(task, opts);
  auto mode = Mode::rw_mode() | Mode::create_bit();
  open(&h->subtask, &h->file, path, mode, h->options);
  h->subtask.on_finished(event::callback([h] { return h->open_complete(); }));
}

void FileSystemImpl::opendir(event::Task* task, File* out,
                             const std::string& path, Mode mode,
                             const base::Options& opts) {
  base::Options o = opts;
  o.get<file::Options>().open_directory = true;
  open(task, out, path, mode, o);
}

void FileSystemImpl::mkdir(event::Task* task, const std::string& path,
                           const base::Options& opts) {
  if (!task->start()) return;
  auto* h = new OpenCloseHelper(task, opts);
  auto mode = Mode::create_exclusive_wo_mode();
  h->options.get<file::Options>().open_directory = true;
  open(&h->subtask, &h->file, path, mode, h->options);
  h->subtask.on_finished(event::callback([h] { return h->open_complete(); }));
}

void FileSystemImpl::rmdir(event::Task* task, const std::string& path,
                           const base::Options& opts) {
  base::Options o = opts;
  o.get<file::Options>().remove_directory = true;
  unlink(task, path, o);
}

base::Result FileSystemImpl::statfs(StatFS* out, const std::string& path,
                                    const base::Options& opts) const {
  event::Task task;
  statfs(&task, out, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result FileSystemImpl::stat(Stat* out, const std::string& path,
                                  const base::Options& opts) const {
  event::Task task;
  stat(&task, out, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result FileSystemImpl::set_stat(const std::string& path,
                                      const SetStat& delta,
                                      const base::Options& opts) {
  event::Task task;
  set_stat(&task, path, delta, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result FileSystemImpl::open(File* out, const std::string& path, Mode mode,
                                  const base::Options& opts) {
  event::Task task;
  open(&task, out, path, mode, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result FileSystemImpl::link(const std::string& oldpath,
                                  const std::string& newpath,
                                  const base::Options& opts) {
  event::Task task;
  link(&task, oldpath, newpath, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result FileSystemImpl::symlink(const std::string& target,
                                     const std::string& linkpath,
                                     const base::Options& opts) {
  event::Task task;
  symlink(&task, target, linkpath, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result FileSystemImpl::unlink(const std::string& path,
                                    const base::Options& opts) {
  event::Task task;
  unlink(&task, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result FileSystemImpl::touch(const std::string& path,
                                   const base::Options& opts) {
  event::Task task;
  touch(&task, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result FileSystemImpl::opendir(File* out, const std::string& path,
                                     Mode mode, const base::Options& opts) {
  event::Task task;
  opendir(&task, out, path, mode, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result FileSystemImpl::mkdir(const std::string& path,
                                   const base::Options& opts) {
  event::Task task;
  mkdir(&task, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result FileSystemImpl::rmdir(const std::string& path,
                                   const base::Options& opts) {
  event::Task task;
  rmdir(&task, path, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

void File::assert_valid() const noexcept {
  CHECK(ptr_) << ": file::File is empty";
}

}  // namespace file
