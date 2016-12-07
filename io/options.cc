// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "io/options.h"

#include <mutex>

#include "base/util.h"

namespace io {

static std::mutex g_mu;
static Options* g_opts = nullptr;

const Options& default_options() {
  auto lock = base::acquire_lock(g_mu);
  if (g_opts == nullptr) g_opts = new Options;
  return *g_opts;
}

void set_default_options(Options o) {
  auto lock = base::acquire_lock(g_mu);
  if (g_opts == nullptr)
    g_opts = new Options(std::move(o));
  else
    *g_opts = std::move(o);
}

}  // namespace io
