// io/util.h - Additional utility functions
// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_UTIL_H
#define IO_UTIL_H

#include <limits>
#include <vector>

#include "base/result.h"
#include "io/reader.h"
#include "io/writer.h"

namespace io {

void copy_n(event::Task* task, std::size_t* copied, std::size_t max, Writer w,
            Reader r, const Options& opts = default_options());

inline base::Result copy_n(std::size_t* copied, std::size_t max, Writer w,
                           Reader r, const Options& opts = default_options()) {
  event::Task task;
  copy_n(&task, copied, max, std::move(w), std::move(r), opts);
  event::wait(opts.manager(), &task);
  return task.result();
}

inline void copy(event::Task* task, std::size_t* copied, Writer w, Reader r,
                 const Options& opts = default_options()) {
  constexpr auto max = std::numeric_limits<std::size_t>::max();
  copy_n(task, copied, max, std::move(w), std::move(r), opts);
}

inline base::Result copy(std::size_t* copied, Writer w, Reader r,
                         const Options& opts = default_options()) {
  constexpr auto max = std::numeric_limits<std::size_t>::max();
  return copy_n(copied, max, std::move(w), std::move(r), opts);
}

}  // namespace io

#endif  // IO_UTIL_H
