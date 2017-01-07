// path/path.h - Tools for manipulating paths
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef PATH_PATH_H
#define PATH_PATH_H

#include <string>
#include <utility>

namespace path {

// Returns true iff the given path is absolute.
inline bool is_abs(const std::string& path) {
  return !path.empty() && path.front() == '/';
}

// Partially cleans up a path.
// - Collapses 'foo//bar' into 'foo/bar'
// - Removes redundant '.' components
// - Does NOT process '..' components
std::string partial_clean(const std::string& path);

// Cleans up a path name according to logical rules.
// - Collapses 'foo//bar' into 'foo/bar'
// - Removes redundant '.' components
// - Collapses 'foo/../bar' into 'bar'
//   - NOTE: This may change the meaning of the path in the face of symlinks!
std::string clean(const std::string& path);

// Splits a path into a parent directory + a base filename.
std::pair<std::string, std::string> split(const std::string& path);

// Returns the parent directory of a path.
inline std::string dirname(const std::string& path) {
  return split(path).first;
}

// Returns the base filename of a path.
inline std::string basename(const std::string& path) {
  return split(path).second;
}

// Joins two paths by concatenating them, separated by '/'.
void join(std::string* head, const std::string& tail);

// Extends path::join to three or more arguments.
template <typename... Rest>
void join(std::string* head, const std::string& first,
          const std::string& second, const Rest&... rest) {
  join(head, first);
  join(head, second, rest...);
}

// Alternative version of path::join that returns its value.
template <typename... Rest>
std::string join(const std::string& first, const std::string& second,
                 const Rest&... rest) {
  std::string out = first;
  join(&out, second, rest...);
  return out;
}

}  // namespace path

#endif  // PATH_PATH_H
