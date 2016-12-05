// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/debug.h"

#include <mutex>

#include "base/util.h"

namespace base {

static std::mutex g_mu;
static bool g_debug =
#ifdef NDEBUG
    false
#else
    true
#endif
    ;

bool debug() noexcept {
  auto lock = acquire_lock(g_mu);
  return g_debug;
}

void set_debug(bool value) {
  auto lock = acquire_lock(g_mu);
  g_debug = value;
}

}  // namespace base
