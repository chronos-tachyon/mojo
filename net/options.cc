// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "net/options.h"

#include <mutex>

#include "base/util.h"

namespace net {

static std::mutex g_mu;
static Options* g_ptr = nullptr;

Options default_options() noexcept {
  auto lock = base::acquire_lock(g_mu);
  if (!g_ptr) g_ptr = new Options;
  return *g_ptr;
}

void set_default_options(Options opts) noexcept {
  auto lock = base::acquire_lock(g_mu);
  if (!g_ptr) g_ptr = new Options;
  *g_ptr = std::move(opts);
}

}  // namespace net