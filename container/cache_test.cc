// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <iostream>

#include "base/concat.h"
#include "base/logging.h"
#include "base/options.h"
#include "base/result_testing.h"
#include "container/cache.h"
#include "event/manager.h"
#include "io/options.h"

struct TLCHelper {
  container::CachePtr cache;
  base::Options options;
  std::size_t hits;
  std::size_t misses;

  TLCHelper(container::CachePtr c, base::Options o) noexcept : cache(std::move(c)), options(std::move(o)), hits(0), misses(0) {}

  void clear() {
    EXPECT_OK(cache->clear(options));
  }

  std::string get(base::StringPiece key) {
    std::string out;
    auto r = cache->get(&out, key, options);
    if (r) {
      ++hits;
    } else {
      ++misses;
      SCOPED_TRACE(key);
      EXPECT_NOT_FOUND(r);
      out.assign("<miss>");
    }
    return out;
  }

  void put(base::StringPiece key, base::StringPiece value) {
    SCOPED_TRACE(key);
    EXPECT_OK(cache->put(key, value, options));
  }

  bool remove(base::StringPiece key) {
    auto r = cache->remove(key, options);
    if (!r) {
      SCOPED_TRACE(key);
      EXPECT_NOT_FOUND(r);
      return false;
    }
    return true;
  }

  container::CacheStats stats() {
    container::CacheStats out;
    EXPECT_OK(cache->stats(&out, options));
    return out;
  }

  bool check(base::StringPiece key, base::StringPiece value) {
    auto str = get(key);
    if (str == "<miss>") {
      put(key, value);
      return false;
    }
    SCOPED_TRACE(base::concat("\"", key, "\" -> \"", value, "\""));
    EXPECT_EQ(str, value);
    return true;
  }

  void Basics() {
    EXPECT_EQ("<miss>", get("a"));
    EXPECT_EQ(0U, stats().num_items);

    put("a", "aaaa");
    EXPECT_EQ("<miss>", get("b"));
    EXPECT_EQ(1U, stats().num_items);

    put("b", "bbbb");
    EXPECT_EQ("<miss>", get("c"));
    EXPECT_EQ(2U, stats().num_items);

    put("c", "cccc");
    EXPECT_EQ("<miss>", get("d"));
    EXPECT_EQ(3U, stats().num_items);

    put("d", "dddd");
    EXPECT_EQ("<miss>", get("e"));
    EXPECT_EQ(4U, stats().num_items);

    put("e", "eeee");
    EXPECT_EQ(4U, stats().num_items);

    check("a", "aaaa");
    check("b", "bbbb");
    check("c", "cccc");
    check("d", "dddd");
    check("e", "eeee");
    check("f", "ffff");
    check("g", "gggg");
    check("h", "hhhh");

    clear();
  }

  void Removal() {
    EXPECT_EQ("<miss>", get("a"));
    put("a", "aaaa");
    EXPECT_EQ("aaaa", get("a"));
    remove("a");
    EXPECT_EQ("<miss>", get("a"));

    check("a", "aaaa");
    check("b", "bbbb");
    check("c", "cccc");
    check("d", "dddd");
    remove("c");
    check("e", "eeee");

    clear();
  }
};

static void TestLocalCache(container::CacheType t) {
  event::Manager m;
  event::ManagerOptions mo;
  mo.set_async_mode();
  ASSERT_OK(event::new_manager(&m, mo));

  base::Options o;
  o.get<io::Options>().manager = m;

  container::CacheOptions co(t, 4);
  container::CachePtr c = container::new_cache(co);

  TLCHelper helper(c, o);

  LOG(INFO) << "[TestLocalCache_Basics:" << t << "]";
  base::log_flush();
  helper.Basics();

  LOG(INFO) << "[TestLocalCache_Removal:" << t << "]";
  base::log_flush();
  helper.Removal();

  LOG(INFO) << "[end:" << t << "]";
  base::log_flush();

  m.shutdown();
}

TEST(Clock, EndToEnd) { TestLocalCache(container::CacheType::clock); }

TEST(LRU, EndToEnd) { TestLocalCache(container::CacheType::lru); }

TEST(CART, EndToEnd) { TestLocalCache(container::CacheType::cart); }
