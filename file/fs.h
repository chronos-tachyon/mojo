// file/fs.h - Definitions for file::{FileSystemImpl,FileImpl,File}
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef FILE_FS_H
#define FILE_FS_H

#include <memory>
#include <string>

#include "base/result.h"
#include "event/task.h"
#include "file/mode.h"
#include "file/options.h"
#include "file/perm.h"
#include "file/stat.h"
#include "io/options.h"
#include "io/reader.h"
#include "io/writer.h"

namespace file {

enum class Whence : uint8_t {
  start = 0,
  current = 1,
  end = 2,
};

class File;            // forward declaration
class FileImpl;        // forward declaration
class FileSystemImpl;  // forward declaration

using FilePtr = std::shared_ptr<FileImpl>;
using FileSystemPtr = std::shared_ptr<FileSystemImpl>;

class FileSystemImpl {
 protected:
  explicit FileSystemImpl(std::string name) noexcept : name_(std::move(name)) {}

  void set_self(std::weak_ptr<FileSystemImpl> self) noexcept {
    self_ = std::move(self);
  }

 public:
  FileSystemImpl() = delete;
  FileSystemImpl(const FileSystemImpl&) = delete;
  FileSystemImpl(FileSystemImpl&&) = delete;
  FileSystemImpl& operator=(const FileSystemImpl&) = delete;
  FileSystemImpl& operator=(FileSystemImpl&&) = delete;

  const std::string& name() const noexcept { return name_; }
  FileSystemPtr self() const noexcept { return self_.lock(); }

  virtual ~FileSystemImpl() noexcept = default;

  virtual void statfs(
      event::Task* task, StatFS* out, const std::string& path,
      const base::Options& opts = base::default_options()) const = 0;

  virtual void stat(
      event::Task* task, Stat* out, const std::string& path,
      const base::Options& opts = base::default_options()) const = 0;

  virtual void set_stat(
      event::Task* task, const std::string& path, const SetStat& delta,
      const base::Options& opts = base::default_options()) = 0;

  virtual void open(event::Task* task, File* out, const std::string& path,
                    Mode mode = Mode::ro_mode(),
                    const base::Options& opts = base::default_options()) = 0;

  virtual void link(event::Task* task, const std::string& oldpath,
                    const std::string& newpath,
                    const base::Options& opts = base::default_options()) = 0;

  virtual void symlink(event::Task* task, const std::string& target,
                       const std::string& linkpath,
                       const base::Options& opts = base::default_options()) = 0;

  virtual void unlink(event::Task* task, const std::string& path,
                      const base::Options& opts = base::default_options()) = 0;

  void touch(event::Task* task, const std::string& path,
             const base::Options& opts = base::default_options());

  void opendir(event::Task* task, File* out, const std::string& path,
               Mode mode = Mode::ro_mode(),
               const base::Options& opts = base::default_options());

  void mkdir(event::Task* task, const std::string& path,
             const base::Options& opts = base::default_options());

  void rmdir(event::Task* task, const std::string& path,
             const base::Options& opts = base::default_options());

  // Synchronous versions of the methods above {{{

  base::Result statfs(
      StatFS* out, const std::string& path,
      const base::Options& opts = base::default_options()) const;

  base::Result stat(Stat* out, const std::string& path,
                    const base::Options& opts = base::default_options()) const;

  base::Result set_stat(
      const std::string& path, const SetStat& delta,
      const base::Options& opts = base::default_options());

  base::Result open(File* out, const std::string& path,
                    Mode mode = Mode::ro_mode(),
                    const base::Options& opts = base::default_options());

  base::Result link(const std::string& oldpath, const std::string& newpath,
                    const base::Options& opts = base::default_options());

  base::Result symlink(
      const std::string& target, const std::string& linkpath,
      const base::Options& opts = base::default_options());

  base::Result unlink(
      const std::string& path,
      const base::Options& opts = base::default_options());

  base::Result touch(const std::string& path,
                     const base::Options& opts = base::default_options());

  base::Result opendir(
      File* out, const std::string& path, Mode mode = Mode::ro_mode(),
      const base::Options& opts = base::default_options());

  base::Result mkdir(const std::string& path,
                     const base::Options& opts = base::default_options());

  base::Result rmdir(const std::string& path,
                     const base::Options& opts = base::default_options());

  // }}}

 private:
  const std::string name_;
  std::weak_ptr<FileSystemImpl> self_;
};

class FileImpl {
 protected:
  FileImpl(FileSystemPtr fs, std::string path, Mode mode) noexcept
      : fs_(std::move(fs)),
        path_(std::move(path)),
        mode_(mode) {}

 public:
  FileImpl() = delete;
  FileImpl(const FileImpl&) = delete;
  FileImpl(FileImpl&&) = delete;
  FileImpl& operator=(const FileImpl&) = delete;
  FileImpl& operator=(FileImpl&&) = delete;

  const FileSystemPtr& filesystem() const noexcept { return fs_; }
  const std::string& path() const noexcept { return path_; }
  Mode mode() const noexcept { return mode_; }

  virtual ~FileImpl() noexcept = default;

  virtual io::Reader reader() = 0;
  virtual io::Writer writer() = 0;

  virtual void readdir(event::Task* task, std::vector<DirEntry>* out,
                       const base::Options& opts) const = 0;

  virtual void statfs(event::Task* task, StatFS* out,
                      const base::Options& opts) const = 0;

  virtual void stat(event::Task* task, Stat* out,
                    const base::Options& opts) const = 0;

  virtual void size(event::Task* task, int64_t* out,
                    const base::Options& opts) const = 0;

  virtual void tell(event::Task* task, int64_t* out,
                    const base::Options& opts) const = 0;

  virtual void set_stat(event::Task* task, const SetStat& delta,
                        const base::Options& opts) = 0;

  virtual void seek(event::Task* task, int64_t off, Whence whence,
                    const base::Options& opts) = 0;

  virtual void truncate_at(event::Task* task, int64_t off,
                           const base::Options& opts) = 0;

  virtual void close(event::Task* task, const base::Options& opts) = 0;

 private:
  const FileSystemPtr fs_;
  const std::string path_;
  const Mode mode_;
};

class File {
 public:
  using Pointer = FilePtr;

  // File is constructible from an implementation.
  File(Pointer ptr) noexcept : ptr_(std::move(ptr)) {}

  // File is default constructible.
  File() noexcept = default;

  // File is copyable and moveable.
  File(const File&) noexcept = default;
  File(File&&) noexcept = default;
  File& operator=(const File&) noexcept = default;
  File& operator=(File&&) noexcept = default;

  // Resets this File to the empty state.
  void reset() noexcept { ptr_.reset(); }

  // Swaps this File with another.
  void swap(File& other) noexcept { ptr_.swap(other.ptr_); }

  // Returns true iff this File is non-empty.
  explicit operator bool() const noexcept { return !!ptr_; }

  // Asserts that this File is non-empty.
  void assert_valid() const noexcept;

  const Pointer& implementation() const noexcept { return ptr_; }
  Pointer& implementation() noexcept { return ptr_; }

  const FileSystemPtr& filesystem() const noexcept {
    assert_valid();
    return ptr_->filesystem();
  }

  const std::string& path() const noexcept {
    assert_valid();
    return ptr_->path();
  }

  Mode mode() const noexcept {
    assert_valid();
    return ptr_->mode();
  }

  io::Reader reader() const {
    assert_valid();
    return ptr_->reader();
  }

  io::Writer writer() const {
    assert_valid();
    return ptr_->writer();
  }

  void readdir(event::Task* task, std::vector<DirEntry>* out,
               const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->readdir(task, out, opts);
  }

  void statfs(event::Task* task, StatFS* out,
              const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->statfs(task, out, opts);
  }

  void stat(event::Task* task, Stat* out,
            const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->stat(task, out, opts);
  }

  void size(event::Task* task, int64_t* out,
            const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->size(task, out, opts);
  }

  void tell(event::Task* task, int64_t* out,
            const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->tell(task, out, opts);
  }

  void set_stat(event::Task* task, const SetStat& delta,
                const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->set_stat(task, delta, opts);
  }

  void seek(event::Task* task, int64_t off, Whence whence = Whence::current,
            const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->seek(task, off, whence, opts);
  }

  void truncate_at(event::Task* task, int64_t off,
                   const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->truncate_at(task, off, opts);
  }

  void truncate(event::Task* task,
                const base::Options& opts = base::default_options()) const {
    truncate_at(task, 0, opts);
  }

  void close(event::Task* task,
             const base::Options& opts = base::default_options()) const {
    assert_valid();
    ptr_->close(task, opts);
  }

  // Synchronous versions of the above

  base::Result readdir(
      std::vector<DirEntry>* out,
      const base::Options& opts = base::default_options()) const {
    event::Task task;
    readdir(&task, out, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  }

  base::Result statfs(
      StatFS* out, const base::Options& opts = base::default_options()) const {
    event::Task task;
    statfs(&task, out, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  }

  base::Result stat(Stat* out,
                    const base::Options& opts = base::default_options()) const {
    event::Task task;
    stat(&task, out, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  }

  base::Result size(int64_t* out,
                    const base::Options& opts = base::default_options()) const {
    event::Task task;
    size(&task, out, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  }

  base::Result tell(int64_t* out,
                    const base::Options& opts = base::default_options()) const {
    event::Task task;
    tell(&task, out, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  }

  base::Result set_stat(
      const SetStat& delta,
      const base::Options& opts = base::default_options()) const {
    event::Task task;
    set_stat(&task, delta, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  }

  base::Result seek(int64_t off, Whence whence = Whence::current,
                    const base::Options& opts = base::default_options()) const {
    event::Task task;
    seek(&task, off, whence, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  }

  base::Result truncate_at(
      int64_t off, const base::Options& opts = base::default_options()) const {
    event::Task task;
    truncate_at(&task, off, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  }

  base::Result truncate(
      const base::Options& opts = base::default_options()) const {
    return truncate_at(0, opts);
  }

  base::Result close(
      const base::Options& opts = base::default_options()) const {
    event::Task task;
    close(&task, opts);
    event::wait(io::get_manager(opts), &task);
    return task.result();
  }

 private:
  Pointer ptr_;
};

inline void swap(File& a, File& b) noexcept { a.swap(b); }
inline bool operator==(const File& a, const File& b) noexcept {
  return a.implementation() == b.implementation();
}
inline bool operator!=(const File& a, const File& b) noexcept {
  return !(a == b);
}

}  // namespace file

#endif  // FILE_FS_H
