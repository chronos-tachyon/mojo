// file/mem.h - In-memory implementation of file::FileSystemImpl
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef FILE_MEM_H
#define FILE_MEM_H

#include <string>

#include "file/fs.h"

namespace file {

FileSystemPtr mem_filesystem(std::string name);

}  // namespace file

#endif  // FILE_MEM_H
