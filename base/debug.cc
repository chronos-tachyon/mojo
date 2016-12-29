// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/debug.h"

#include <atomic>

namespace base {

static constexpr bool kDebugDefault =
#ifdef NDEBUG
    false
#else
    true
#endif
    ;

static std::atomic_bool g_debug(kDebugDefault);

bool debug() noexcept { return g_debug.load(std::memory_order_relaxed); }

void set_debug(bool value) noexcept {
  g_debug.store(value, std::memory_order_relaxed);
}

}  // namespace base
