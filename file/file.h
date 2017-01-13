// file/file.h - Wrappers for file::system_registry()
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef FILE_FILE_H
#define FILE_FILE_H

#include "file/fs.h"
#include "file/registry.h"

namespace file {

FileSystemPtr find(const std::string& fsname);

void statfs(event::Task* task, StatFS* out, const std::string& fsname,
            const std::string& path,
            const base::Options& opts = base::default_options());

void stat(event::Task* task, Stat* out, const std::string& fsname,
          const std::string& path,
          const base::Options& opts = base::default_options());

void set_stat(event::Task* task, const std::string& fsname,
              const std::string& path, const SetStat& delta,
              const base::Options& opts = base::default_options());

void open(event::Task* task, File* out, const std::string& fsname,
          const std::string& path, Mode mode = Mode::ro_mode(),
          const base::Options& opts = base::default_options());

void link(event::Task* task, const std::string& fsname,
          const std::string& oldpath, const std::string& newpath,
          const base::Options& opts = base::default_options());

void symlink(event::Task* task, const std::string& fsname,
             const std::string& target, const std::string& linkpath,
             const base::Options& opts = base::default_options());

void unlink(event::Task* task, const std::string& fsname,
            const std::string& path,
            const base::Options& opts = base::default_options());

void touch(event::Task* task, const std::string& fsname,
           const std::string& path,
           const base::Options& opts = base::default_options());

void opendir(event::Task* task, File* out, const std::string& fsname,
             const std::string& path, Mode mode = Mode::ro_mode(),
             const base::Options& opts = base::default_options());

void mkdir(event::Task* task, const std::string& fsname,
           const std::string& path,
           const base::Options& opts = base::default_options());

void rmdir(event::Task* task, const std::string& fsname,
           const std::string& path,
           const base::Options& opts = base::default_options());

// Synchronous versions of the methods above {{{

base::Result statfs(StatFS* out, const std::string& fsname,
                    const std::string& path,
                    const base::Options& opts = base::default_options());

base::Result stat(Stat* out, const std::string& fsname, const std::string& path,
                  const base::Options& opts = base::default_options());

base::Result set_stat(const std::string& fsname, const std::string& path,
                      const SetStat& delta,
                      const base::Options& opts = base::default_options());

base::Result open(File* out, const std::string& fsname, const std::string& path,
                  Mode mode = Mode::ro_mode(),
                  const base::Options& opts = base::default_options());

base::Result link(const std::string& fsname, const std::string& oldpath,
                  const std::string& newpath,
                  const base::Options& opts = base::default_options());

base::Result symlink(const std::string& fsname, const std::string& target,
                     const std::string& linkpath,
                     const base::Options& opts = base::default_options());

base::Result unlink(const std::string& fsname, const std::string& path,
                    const base::Options& opts = base::default_options());

base::Result touch(const std::string& fsname, const std::string& path,
                   const base::Options& opts = base::default_options());

base::Result opendir(File* out, const std::string& fsname,
                     const std::string& path, Mode mode = Mode::ro_mode(),
                     const base::Options& opts = base::default_options());

base::Result mkdir(const std::string& fsname, const std::string& path,
                   const base::Options& opts = base::default_options());

base::Result rmdir(const std::string& fsname, const std::string& path,
                   const base::Options& opts = base::default_options());

// }}}

}  // namespace file

#endif  // FILE_FILE_H
