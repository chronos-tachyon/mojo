// io/ratelimiter.h - Rate-limited I/O
// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef IO_RATELIMITER_H
#define IO_RATELIMITER_H

#include <memory>

#include "base/options.h"
#include "base/time/duration.h"
#include "event/task.h"
#include "io/reader.h"
#include "io/writer.h"

namespace io {

class RateLimiterImpl {
 protected:
  RateLimiterImpl() noexcept = default;

 public:
  RateLimiterImpl(const RateLimiterImpl&) = delete;
  RateLimiterImpl(RateLimiterImpl&&) = delete;
  RateLimiterImpl& operator=(const RateLimiterImpl&) = delete;
  RateLimiterImpl& operator=(RateLimiterImpl&&) = delete;

  virtual ~RateLimiterImpl() noexcept = default;

  virtual void gate(event::Task* task, std::size_t n,
                    const base::Options& opts = base::default_options()) = 0;

  base::Result gate(std::size_t n,
                    const base::Options& opts = base::default_options());
};

using RateLimiter = std::shared_ptr<RateLimiterImpl>;

RateLimiter new_ratelimiter(base::time::Duration window, std::size_t count,
                            std::size_t burst = 0);

Reader ratelimitedreader(Reader r, RateLimiter l);
Writer ratelimitedwriter(Writer w, RateLimiter l);

}  // namespace io

#endif  // IO_RATELIMITER_H
