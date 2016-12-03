// base/util.h - Miscellaneous small utility functions
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_UTIL_H
#define BASE_UTIL_H

#include <mutex>

namespace base {

using Lock = std::unique_lock<std::mutex>;

inline Lock acquire_lock(std::mutex& mu) {
  return Lock(mu);
}

}  // namespace base

#endif // BASE_UTIL_H
