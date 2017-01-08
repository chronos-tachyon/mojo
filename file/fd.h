// file/fd.h - Local (native) implementation of file::File
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef FILE_FD_H
#define FILE_FD_H

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>

#include <string>

#include "base/fd.h"
#include "file/fs.h"

namespace file {

base::Result convert_statfs(StatFS* out, const struct statfs& f);
base::Result convert_stat(Stat* out, const struct stat& st);

File fdfile(FileSystemPtr fs, std::string path, Mode mode, base::FD fd);

}  // namespace file

#endif  // FILE_FD_H
