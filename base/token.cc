// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/token.h"

#include <mutex>

namespace base {

static std::mutex g_tok_mu;
static uint64_t g_tok_last = 0;

token_t next_token() noexcept {
  std::lock_guard<std::mutex> lock(g_tok_mu);
  return token_t(++g_tok_last);
}

}  // namespace base
