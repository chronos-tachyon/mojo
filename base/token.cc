// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/token.h"

#include <atomic>
#include <cstdint>

namespace base {

static std::atomic<uint64_t> g_last;

token_t next_token() noexcept {
  return token_t(1 + g_last.fetch_add(1));
}

}  // namespace base
