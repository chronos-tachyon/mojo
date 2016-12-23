// io/common.h - Stuff common to both io::Readers and io::Writers
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_COMMON_H
#define IO_COMMON_H

#include <functional>

#include "base/result.h"
#include "event/task.h"
#include "io/options.h"

namespace io {

using CloseFn = std::function<void(event::Task*, const io::Options& opts)>;
using SyncCloseFn = std::function<base::Result(const io::Options& opts)>;

struct NoOpClose {
  void operator()(event::Task* task, const io::Options& opts) const {
    if (task->start()) task->finish_ok();
  }
  base::Result operator()(const io::Options& opts) const {
    return base::Result();
  }
};

}  // namespace io

#endif  // IO_COMMON_H
