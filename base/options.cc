// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "base/options.h"

#include <mutex>

#include "base/mutex.h"

namespace base {

Options::Options(const Options& other) {
  for (const auto& pair : other.map_) {
    map_[pair.first] = pair.second->copy();
  }
}

Options& Options::operator=(const Options& other) {
  map_.clear();
  for (const auto& pair : other.map_) {
    map_[pair.first] = pair.second->copy();
  }
  return *this;
}

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

}  // namespace base
