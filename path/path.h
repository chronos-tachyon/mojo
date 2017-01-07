// path/path.h - Tools for manipulating paths
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef PATH_PATH_H
#define PATH_PATH_H

#include <string>

namespace path {

// Cleans up a logical path name
std::string clean(const std::string& path);

}  // namespace path

#endif  // PATH_PATH_H
