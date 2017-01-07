// path/path.h - Tools for manipulating paths
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef PATH_PATH_H
#define PATH_PATH_H

#include <string>
#include <utility>

namespace path {

// Cleans up a logical path name
std::string clean(const std::string& path);

// Splits a path into a directory path + a base filename
std::pair<std::string, std::string> split(const std::string& path);

inline std::string dirname(const std::string& path) {
  return split(path).first;
}

inline std::string basename(const std::string& path) {
  return split(path).second;
}

}  // namespace path

#endif  // PATH_PATH_H
