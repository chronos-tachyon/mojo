// file/local.h - Local (native) implementation of file::FileSystemImpl
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef FILE_LOCAL_H
#define FILE_LOCAL_H

#include "file/fs.h"

namespace file {

FileSystemPtr local_filesystem();

}  // namespace file

#endif  // FILE_LOCAL_H
