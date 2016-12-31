// base/debug.h - Provides a friendly way to check the global debugging mode
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_DEBUG_H
#define BASE_DEBUG_H

namespace base {

bool debug() noexcept;
void set_debug(bool value) noexcept;

}  // namespace base

#endif  // BASE_DEBUG_H
