// base/debug.h - Provides a C++11-friendly way to access the NDEBUG macro
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef BASE_DEBUG_H
#define BASE_DEBUG_H

namespace base {

static constexpr bool debug =
#ifdef NDEBUG
    false
#else
    true
#endif
    ;

}  // namespace base

#endif  // BASE_DEBUG_H
