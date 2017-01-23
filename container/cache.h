// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#ifndef CONTAINER_CACHE_H
#define CONTAINER_CACHE_H

#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/options.h"
#include "base/result.h"
#include "base/strings.h"
#include "event/task.h"

namespace container {

struct CacheStats {
  std::size_t num_items = 0;
  std::size_t num_bytes = 0;
};

class Cache {
 protected:
  Cache() noexcept = default;

 public:
  Cache(const Cache&) = delete;
  Cache(Cache&&) = delete;
  Cache& operator=(const Cache&) = delete;
  Cache& operator=(Cache&&) = delete;

  virtual ~Cache() noexcept = default;

  virtual void clear(event::Task* task,
                     const base::Options& opts = base::default_options()) = 0;

  virtual void get(event::Task* task, std::string* out, base::StringPiece key,
                   const base::Options& opts = base::default_options()) = 0;

  virtual void put(event::Task* task, base::StringPiece key,
                   base::StringPiece value,
                   const base::Options& opts = base::default_options()) = 0;

  virtual void remove(event::Task* task, base::StringPiece key,
                      const base::Options& opts = base::default_options()) = 0;

  virtual void stats(event::Task* task, CacheStats* out,
                     const base::Options& opts = base::default_options()) = 0;

  virtual void visualize(
      event::Task* task, std::string* out,
      const base::Options& opts = base::default_options()) const = 0;

  // Synchronous versions of the above functions {{{
  base::Result clear(const base::Options& opts = base::default_options());

  base::Result get(std::string* out, base::StringPiece key,
                   const base::Options& opts = base::default_options());

  base::Result put(base::StringPiece key, base::StringPiece value,
                   const base::Options& opts = base::default_options());

  base::Result remove(base::StringPiece key,
                      const base::Options& opts = base::default_options());

  base::Result stats(CacheStats* out,
                     const base::Options& opts = base::default_options());

  base::Result visualize(std::string* out, const base::Options& opts =
                                               base::default_options()) const;
  // }}}
};

using CachePtr = std::shared_ptr<Cache>;

enum class CacheType : uint8_t {
  clock = 0,
  lru = 1,
  cart = 2,

  best_available = 255,
};

void append_to(std::string* out, CacheType type);
inline std::size_t length_hint(CacheType type) noexcept { return 14; }
std::ostream& operator<<(std::ostream& o, CacheType type);

struct CacheOptions {
  CacheType type;
  std::size_t max_items;
  std::size_t max_bytes;

  explicit CacheOptions(CacheType type = CacheType::best_available,
                        std::size_t max_items = 1024,
                        std::size_t max_bytes = SIZE_MAX) noexcept
      : type(type),
        max_items(max_items),
        max_bytes(max_bytes) {}

  explicit CacheOptions(std::size_t max_items,
                        std::size_t max_bytes = SIZE_MAX) noexcept
      : CacheOptions(CacheType::best_available, max_items, max_bytes) {}
};

CachePtr new_cache(const CacheOptions& co);

}  // namespace container

#endif  // CONTAINER_CACHE_H
