// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "container/cache.h"

#include <algorithm>
#include <deque>
#include <ostream>
#include <vector>

#include "base/backport.h"
#include "base/debug.h"
#include "base/logging.h"
#include "io/options.h"

namespace container {

inline namespace implementation {
static std::string shrink(base::StringPiece sp) {
  std::string str(sp.as_string());
  str.shrink_to_fit();
  return str;
}

__attribute__((const)) static inline std::size_t min(std::size_t a,
                                                     std::size_t b) noexcept {
  return (a < b) ? a : b;
}

__attribute__((const)) static inline std::size_t max(std::size_t a,
                                                     std::size_t b) noexcept {
  return (a > b) ? a : b;
}

struct Item {
  const std::string key;
  std::string value;
  bool dead;
  bool used;
  bool longterm;

  static std::unique_ptr<Item> make(base::StringPiece k) {
    return base::backport::make_unique<Item>(k);
  }

  static std::size_t byte_size(base::StringPiece k,
                               base::StringPiece v) noexcept {
    return sizeof(Item) + k.size() + v.size();
  }

  explicit Item(base::StringPiece k)
      : key(shrink(k)), dead(false), used(false), longterm(false) {}

  std::size_t byte_size() const noexcept { return byte_size(key, value); }

  void kill() {
    dead = true;
    value.clear();
    value.shrink_to_fit();
  }

  void assign(base::StringPiece v) {
    DCHECK(!dead);
    value = shrink(v);
  }
};

using ItemPtr = std::unique_ptr<Item>;

static void visualize_slot(std::string* out, base::StringPiece prefix,
                           const ItemPtr& slot) {
  if (slot) {
    base::StringPiece dead, used, lifetime;
    if (slot->dead) dead = " [dead]";
    if (slot->used) used = " [used]";
    if (slot->longterm)
      lifetime = " [L]";
    else
      lifetime = " [S]";
    base::concat_to(out, prefix, " \"", slot->key, "\" = ");
    if (slot->value.empty())
      base::concat_to(out, "\"\"");
    else
      base::concat_to(out, "... (", slot->value.size(), " bytes)");
    base::concat_to(out, dead, used, lifetime, ",\n");
  } else {
    base::concat_to(out, prefix, " NULL,\n");
  }
}

static void visualize_clock(std::string* out, base::StringPiece name,
                            const ItemPtr* p, const ItemPtr* q,
                            const ItemPtr* hand) {
  if (p == q) {
    base::concat_to(out, name, " = []\n");
    return;
  }
  base::concat_to(out, name, " = [\n");
  while (p != q) {
    const ItemPtr& slot = *p;
    base::StringPiece prefix = "   ";
    if (p == hand) prefix = " ->";
    visualize_slot(out, prefix, slot);
    ++p;
  }
  base::concat_to(out, "]\n");
}

template <typename ConstIt>
static void visualize_lru(std::string* out, base::StringPiece name, ConstIt p,
                          ConstIt q) {
  if (p == q) {
    base::concat_to(out, name, " = []\n");
    return;
  }
  base::concat_to(out, name, " = [\n");
  ConstIt it = p;
  while (it != q) {
    const ItemPtr& slot = *it;
    base::StringPiece prefix;
    if (it == p)
      prefix = "  M";
    else if (it + 1 == q)
      prefix = "  L";
    else
      prefix = "   ";
    visualize_slot(out, prefix, slot);
    ++it;
  }
  base::concat_to(out, "]\n");
}

static void visualize_param(std::string* out, base::StringPiece name,
                            std::size_t value) {
  base::concat_to(out, name, " = ", value, "\n");
}

// LocalCacheBase {{{

class LocalCacheBase : public Cache {
 public:
  void clear(event::Task* task, const base::Options& opts) override;

  void get(event::Task* task, std::string* out, base::StringPiece key,
           const base::Options& opts) override;

  void put(event::Task* task, base::StringPiece key, base::StringPiece value,
           const base::Options& opts) override;

  void remove(event::Task* task, base::StringPiece key,
              const base::Options& opts) override;

  void stats(event::Task* task, CacheStats* out,
             const base::Options& opts) override;

 protected:
  explicit LocalCacheBase(std::size_t max_items, std::size_t max_bytes)
      : maxi_(max_items), maxb_(max_bytes), numi_(0), numb_(0) {
    CHECK_GT(max_items, 0U);
    CHECK_GT(max_bytes, 0U);
    map_.reserve(max_items);
  }

  std::size_t num_items() const noexcept { return numi_; }
  std::size_t num_bytes() const noexcept { return numb_; }
  std::size_t max_items() const noexcept { return maxi_; }
  std::size_t max_bytes() const noexcept { return maxb_; }

  void mark_evicted(Item* item) {
    std::size_t n = item->byte_size();
    DCHECK_GE(numb_, n);
    --numi_;
    numb_ -= n;
  }

  void mark_forgotten(Item* item) {
    map_.erase(item->key);
  }

  virtual void clear() = 0;
  virtual void evict_one(Item* item) = 0;
  virtual void evict_any() = 0;
  virtual void place(ItemPtr item) = 0;
  virtual void replace(Item* item) = 0;
  virtual void touch(Item* item) = 0;

 private:
  void evict();

  const std::size_t maxi_;
  const std::size_t maxb_;
  std::size_t numi_;
  std::size_t numb_;
  std::unordered_map<base::StringPiece, Item*> map_;
};

void LocalCacheBase::clear(event::Task* task, const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  clear();
  map_.clear();
  numi_ = 0;
  numb_ = 0;
  task->finish_ok();
}

void LocalCacheBase::get(event::Task* task, std::string* out,
                         base::StringPiece key, const base::Options& opts) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  out->clear();

  auto it = map_.find(key);
  if (it == map_.end()) {
    task->finish(base::Result::not_found());
    return;
  }

  Item* item = it->second;
  if (item->dead) {
    task->finish(base::Result::not_found());
    return;
  }

  touch(item);
  out->append(item->value);
  task->finish_ok();
}

void LocalCacheBase::put(event::Task* task, base::StringPiece key,
                         base::StringPiece value, const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  std::size_t new_size = Item::byte_size(key, value);
  if (max_bytes() < new_size) {
    task->finish(base::Result::out_of_range("item too large"));
    return;
  }

  DCHECK_LE(num_items(), max_items());
  DCHECK_LE(num_bytes(), max_bytes());
  auto it = map_.find(key);
  if (it == map_.end()) {
    auto ptr = Item::make(key);
    auto item = ptr.get();

    while (numi_ >= maxi_) evict();
    item->assign(value);
    ++numi_;
    numb_ += new_size;
    map_[item->key] = item;
    place(std::move(ptr));
  } else {
    auto item = it->second;
    if (item->dead) {
      while (numi_ >= maxi_) evict();
      replace(item);
      ++numi_;
    } else {
      auto old_size = item->byte_size();
      DCHECK_GE(numb_, old_size);
      numb_ -= old_size;
    }
    item->assign(value);
    numb_ += new_size;
  }
  while (numb_ > maxb_) evict();
  DCHECK_LE(num_items(), max_items());
  DCHECK_LE(num_bytes(), max_bytes());
  task->finish_ok();
}

void LocalCacheBase::remove(event::Task* task, base::StringPiece key,
                            const base::Options& opts) {
  CHECK_NOTNULL(task);
  if (!task->start()) return;

  auto it = map_.find(key);
  if (it == map_.end()) {
    task->finish(base::Result::not_found());
    return;
  }

  Item* item = it->second;
  DCHECK_GE(numi_, 1U);
  DCHECK_GE(numb_, item->byte_size());
  evict_one(item);
  task->finish_ok();
}

void LocalCacheBase::stats(event::Task* task, CacheStats* out,
                           const base::Options& opts) {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;

  CacheStats tmp;
  tmp.num_items = num_items();
  tmp.num_bytes = num_bytes();
  *out = tmp;
  task->finish_ok();
}

void LocalCacheBase::evict() {
  DCHECK_GT(numi_, 0U);
  DCHECK_GT(numb_, 0U);
  evict_any();
  DCHECK_LT(numi_, max_items());
}

// }}}
// Clock {{{

class Clock : public LocalCacheBase {
 public:
  explicit Clock(std::size_t max_items, std::size_t max_bytes)
      : LocalCacheBase(max_items, max_bytes), vec_(max_items), hand_(0) {}

  void visualize(event::Task* task, std::string* out,
                 const base::Options& opts) const override;

 protected:
  void clear() override;
  void evict_one(Item* item) override;
  void evict_any() override;
  void place(ItemPtr item) override;
  void replace(Item* item) override;
  void touch(Item* item) override;

 private:
  std::vector<ItemPtr> vec_;
  std::size_t hand_;
};

void Clock::clear() {
  for (auto& slot : vec_) slot.reset();
  hand_ = 0;
}

void Clock::evict_one(Item* item) {
  ItemPtr* const begin = vec_.data();
  ItemPtr* const hand = begin + hand_;
  ItemPtr* const end = begin + vec_.size();

  for (ItemPtr* p = hand; p != end; ++p) {
    ItemPtr& slot = *p;
    if (slot.get() == item) {
      mark_evicted(item);
      mark_forgotten(item);
      slot.reset();
      if (p != hand) std::move_backward(hand, p, p + 1);
      return;
    }
  }
  for (ItemPtr* p = begin; p != hand; ++p) {
    ItemPtr& slot = *p;
    if (slot.get() == item) {
      mark_evicted(item);
      mark_forgotten(item);
      slot.reset();
      std::move(p + 1, hand, p);
      --hand_;
      return;
    }
  }
  LOG(DFATAL) << "BUG! Item in map_ but not in cache";
}

void Clock::evict_any() {
  while (true) {
    ItemPtr& slot = vec_[hand_];
    if (slot && !slot->used) {
      mark_evicted(slot.get());
      mark_forgotten(slot.get());
      slot.reset();
      return;
    }
    if (slot) slot->used = false;
    hand_ = (hand_ + 1) % max_items();
  }
}

void Clock::place(ItemPtr item) {
  ItemPtr& slot = vec_[hand_];
  CHECK(slot == nullptr);
  slot = std::move(item);
  hand_ = (hand_ + 1) % max_items();
}

void Clock::replace(Item* item) {}

void Clock::touch(Item* item) { item->used = true; }

void Clock::visualize(event::Task* task, std::string* out,
                      const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  out->clear();

  const ItemPtr* p = vec_.data();
  const ItemPtr* q = p + max_items();
  const ItemPtr* hand = p + hand_;
  visualize_clock(out, "Clock", p, q, hand);
  task->finish_ok();
}

// }}}
// LRU {{{

class LRU : public LocalCacheBase {
 public:
  explicit LRU(std::size_t max_items, std::size_t max_bytes)
      : LocalCacheBase(max_items, max_bytes) {}

  void visualize(event::Task* task, std::string* out,
                 const base::Options& opts) const override;

 protected:
  void clear() override;
  void evict_one(Item* item) override;
  void evict_any() override;
  void place(ItemPtr item) override;
  void replace(Item* item) override;
  void touch(Item* item) override;

 private:
  std::deque<ItemPtr> q_;
};

void LRU::clear() { q_.clear(); }

void LRU::evict_one(Item* item) {
  for (auto it = q_.begin(), end = q_.end(); it != end; ++it) {
    ItemPtr& slot = *it;
    if (slot.get() == item) {
      mark_evicted(item);
      mark_forgotten(item);
      slot.reset();
      q_.erase(it);
      return;
    }
  }
  LOG(DFATAL) << "BUG! Item in map_ but not in cache";
}

void LRU::evict_any() {
  ItemPtr ptr = std::move(q_.back());
  q_.pop_back();
  mark_evicted(ptr.get());
  mark_forgotten(ptr.get());
}

void LRU::place(ItemPtr item) { q_.push_front(std::move(item)); }

void LRU::replace(Item* item) {}

void LRU::touch(Item* item) {
  for (auto it = q_.begin(), end = q_.end(); it != end; ++it) {
    ItemPtr& slot = *it;
    if (slot.get() == item) {
      ItemPtr tmp = std::move(slot);
      q_.erase(it);
      q_.push_front(std::move(tmp));
      return;
    }
  }
  LOG(DFATAL) << "BUG! Item in map_ but not in cache";
}

void LRU::visualize(event::Task* task, std::string* out,
                    const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  out->clear();

  visualize_lru(out, "LRU", q_.begin(), q_.end());
  task->finish_ok();
}

// }}}
// CART {{{

// CART - Clock with Adaptive Replacement and Temporal filtering
// https://www.usenix.org/conference/fast-04/car-clock-adaptive-replacement
// https://www.usenix.org/legacy/publications/library/proceedings/fast04/tech/full_papers/bansal/bansal.pdf
class CART : public LocalCacheBase {
 public:
  explicit CART(std::size_t max_items, std::size_t max_bytes)
      : LocalCacheBase(max_items, max_bytes),
        vec_(max_items),
        split_(max_items),
        t1hand_(0),
        t2hand_(0),
        p_(0),
        q_(0),
        nn_(max_items),
        ns_(0),
        nl_(0) {}

  void visualize(event::Task* task, std::string* out,
                 const base::Options& opts) const override;

 protected:
  void clear() override;
  void evict_one(Item* item) override;
  void evict_any() override;
  void place(ItemPtr item) override;
  void replace(Item* item) override;
  void touch(Item* item) override;

 private:
  // T1 {{{

  std::size_t t1_size() const noexcept { return split_ - nn_; }
  ItemPtr& t1_head() noexcept {
    DCHECK_GT(split_, 0U);
    DCHECK_GE(t1hand_, 0U);
    DCHECK_LT(t1hand_, split_);
    return vec_[t1hand_];
  }

  void t1_wrap() noexcept {
    if (split_ == 0)
      t1hand_ = 0;
    else
      t1hand_ %= split_;
  }

  void t1_advance() noexcept {
    ++t1hand_;
    t1_wrap();
  }

  void t1_regress() noexcept {
    if (t1hand_ == 0) t1hand_ += split_;
    --t1hand_;
  }

  // }}}
  // T2 {{{

  std::size_t t2_size() const noexcept { return max_items() - split_; }
  ItemPtr& t2_head() noexcept {
    DCHECK_LT(split_, vec_.size());
    DCHECK_GE(t2hand_, 0U);
    DCHECK_LT(split_ + t2hand_, vec_.size());
    return vec_[split_ + t2hand_];
  }

  void t2_wrap() noexcept {
    std::size_t n = vec_.size() - split_;
    if (n == 0)
      t2hand_ = 0;
    else
      t2hand_ %= n;
  }

  void t2_advance() noexcept {
    ++t2hand_;
    t2_wrap();
  }

  // }}}

  void grow_p() noexcept {
    auto x = max(1, ns_ / b1_.size());
    p_ = min(p_ + x, max_items());
  }
  void shrink_p() noexcept {
    auto x = max(1, nl_ / b2_.size());
    if (p_ > x)
      p_ -= x;
    else
      p_ = 0;
  }

  void grow_q() noexcept { q_ = min(q_ + 1, 2 * max_items() - t1_size()); }
  void shrink_q() noexcept {
    auto x = max_items() - t1_size();
    if (q_ > x + 1)
      --q_;
    else
      q_ = x;
  }

  void assert_invariants() const noexcept;
  void move_t1_index_to_t1_tail(std::size_t i) noexcept;
  void move_t2_index_to_t1_tail(std::size_t i) noexcept;
  void move_dead_to_t1_tail(ItemPtr incoming) noexcept;
  void move_t2_head_to_t1_tail() noexcept;
  void move_t1_head_to_t2_tail() noexcept;

  std::vector<ItemPtr> vec_;  // T1: [0..split_); T2: [split_..end)
  std::deque<ItemPtr> b1_;    // B1
  std::deque<ItemPtr> b2_;    // B2
  std::size_t split_;         // T1 vs T2 divider
  std::size_t t1hand_;        // T1 clock hand
  std::size_t t2hand_;        // T2 clock hand
  std::size_t p_;             // T1 target size
  std::size_t q_;             // B1 target size
  std::size_t nn_;            // T1 # of free slots
  std::size_t ns_;            // # of occupied T1 slots not marked longterm
  std::size_t nl_;            // # of occupied T1+T2 slots marked longterm
};

void CART::clear() {
  assert_invariants();

  for (auto& slot : vec_) slot.reset();
  b1_.clear();
  b2_.clear();
  split_ = max_items();
  t1hand_ = 0;
  t2hand_ = 0;
  p_ = 0;
  q_ = 0;
  nn_ = max_items();
  ns_ = 0;
  nl_ = 0;

  assert_invariants();
}

void CART::evict_one(Item* item) {
  assert_invariants();

  for (std::size_t i = 0, n = vec_.size(); i < n; ++i) {
    ItemPtr& slot = vec_[i];
    if (slot.get() == item) {
      bool longterm = slot->longterm;
      mark_evicted(item);
      mark_forgotten(item);
      slot.reset();
      if (longterm)
        --nl_;
      else
        --ns_;
      ++nn_;
      if (i >= split_)
        move_t2_index_to_t1_tail(i);
      else
        move_t1_index_to_t1_tail(i);
      t1_regress();  // Back up t1_head by 1, so that t1_head is null
      goto finish;
    }
  }

  for (auto it = b1_.begin(), end = b1_.end(); it != end; ++it) {
    ItemPtr& slot = *it;
    if (slot.get() == item) {
      mark_forgotten(item);
      b1_.erase(it);
      goto free_a_slot;
    }
  }

  for (auto it = b2_.begin(), end = b2_.end(); it != end; ++it) {
    ItemPtr& slot = *it;
    if (slot.get() == item) {
      mark_forgotten(item);
      b2_.erase(it);
      goto free_a_slot;
    }
  }

  LOG(DFATAL) << "BUG! Item in map_ but not in cache";

free_a_slot:
  evict_any();

finish:
  assert_invariants();
  DCHECK(t1_head() == nullptr);
}

void CART::evict_any() {
  assert_invariants();

  // If the cache is not full, skip ahead and pick an item to evict.
  if (nn_ > 0) goto skip_aging;

  // Bansal Fig. 3 lines 23-26
  while (t2_size() > 0) {
    ItemPtr& slot = t2_head();
    if (!slot->used) break;
    slot->used = false;
    move_t2_head_to_t1_tail();
    // |T2| + |B2| + |T1| - ns >= c
    // |B2| + |T1| + |T2| >= c + ns
    // |B2| + c - nn >= c + ns
    // |B2| + c >= c + ns + nn
    // |B2| >= ns + nn
    if (b2_.size() >= nn_ + ns_) grow_q();
  }

  // Bansal Fig. 3 lines 27-35
  while (t1_size() > 0) {
    ItemPtr& slot = t1_head();
    if (slot->used) {
      slot->used = false;
      t1_advance();
      if (!slot->longterm && t1_size() >= min(p_ + 1, b1_.size())) {
        slot->longterm = true;
        ++nl_;
        --ns_;
      }
    } else if (slot->longterm) {
      move_t1_head_to_t2_tail();
      shrink_q();
    } else {
      break;
    }
  }

skip_aging:
  // Bansal Fig. 3 lines 36-40
  if (t1_size() >= max(1, p_)) {
    ItemPtr& slot = t1_head();
    mark_evicted(slot.get());
    slot->kill();
    b1_.push_front(std::move(slot));
    --ns_;
    ++nn_;
  } else {
    ItemPtr& slot = t2_head();
    mark_evicted(slot.get());
    slot->kill();
    b2_.push_front(std::move(slot));
    move_t2_head_to_t1_tail();
    --nl_;
    ++nn_;
    t1_regress();  // Back up t1_head by 1, so that t1_head is null
  }

  // Bansal Fig. 3 lines 6-10
  if (nn_ == 1 && b1_.size() + b2_.size() > max_items()) {
    std::deque<ItemPtr>* queue = nullptr;
    if (b1_.size() > q_ || b2_.empty()) {
      queue = &b1_;
    } else {
      queue = &b2_;
    }
    auto ptr = std::move(queue->back());
    queue->pop_back();
    mark_forgotten(ptr.get());
  }

  if (t2_size() + b2_.size() > max_items()) {
    auto ptr = std::move(b2_.back());
    b2_.pop_back();
    mark_forgotten(ptr.get());
  }

  // Postconditions:
  assert_invariants();
  DCHECK(t1_head() == nullptr);
}

void CART::place(ItemPtr item) {
  // Precondition: if this line was reached, then x ∉ (B1 ⋃ B2)

  assert_invariants();

  // Bansal Fig. 3 lines 12-13
  ItemPtr& slot = t1_head();
  DCHECK(slot == nullptr);
  DCHECK(!item->dead);
  DCHECK(!item->used);
  DCHECK(!item->longterm);
  slot = std::move(item);
  --nn_;
  ++ns_;
  t1_advance();

  assert_invariants();
}

void CART::replace(Item* item) {
  // Precondition: if this line was reached, then x ∈ (B1 ⋃ B2)

  assert_invariants();

  if (item->longterm) {
    // Bansal Fig. 3 lines 18-20
    for (auto it = b2_.begin(), end = b2_.end(); it != end; ++it) {
      if (it->get() != item) continue;
      auto resurrected = std::move(*it);
      shrink_p();
      item->dead = false;
      b2_.erase(it);
      move_dead_to_t1_tail(std::move(resurrected));
      ++nl_;
      --nn_;
      // |T2| + |B2| + |T1| - ns >= c
      // |B2| + |T1| + |T2| >= c + ns
      // |B2| + c - nn >= c + ns
      // |B2| + c >= c + ns + nn
      // |B2| >= ns + nn
      if (b2_.size() >= nn_ + ns_) grow_q();
      assert_invariants();
      return;
    }
  } else {
    // Bansal Fig. 3 lines 15-16
    for (auto it = b1_.begin(), end = b1_.end(); it != end; ++it) {
      if (it->get() != item) continue;
      auto resurrected = std::move(*it);
      grow_p();
      item->dead = false;
      item->longterm = true;
      b1_.erase(it);
      move_dead_to_t1_tail(std::move(resurrected));
      ++nl_;
      --nn_;
      assert_invariants();
      return;
    }
  }
  LOG(DFATAL) << "BUG! Item in map_ but not in cache history";
}

void CART::touch(Item* item) { item->used = true; }

void CART::visualize(event::Task* task, std::string* out,
                     const base::Options& opts) const {
  CHECK_NOTNULL(task);
  CHECK_NOTNULL(out);
  if (!task->start()) return;
  out->clear();

  const ItemPtr* p = vec_.data();
  const ItemPtr* q = p + split_;
  const ItemPtr* r = p + max_items();
  const ItemPtr* hand1 = p + t1hand_;
  const ItemPtr* hand2 = q + t2hand_;
  visualize_clock(out, "T1", p, q, hand1);
  visualize_clock(out, "T2", q, r, hand2);
  visualize_lru(out, "B1", b1_.begin(), b1_.end());
  visualize_lru(out, "B2", b2_.begin(), b2_.end());
  visualize_param(out, "p", p_);
  visualize_param(out, "q", q_);
  visualize_param(out, "nn", nn_);
  visualize_param(out, "ns", ns_);
  visualize_param(out, "nl", nl_);
  task->finish_ok();
}

void CART::assert_invariants() const noexcept {
#ifndef NDEBUG
  // These (x >= 0) checks are outright impossible if size_t is unsigned.
  CHECK_GE(split_, 0U);
  CHECK_GE(t1hand_, 0U);
  CHECK_GE(t2hand_, 0U);
  CHECK_GE(p_, 0U);
  CHECK_GE(q_, 0U);
  CHECK_GE(nn_, 0U);
  CHECK_GE(ns_, 0U);
  CHECK_GE(nl_, 0U);

  CHECK_EQ(vec_.size(), max_items());
  CHECK_LE(split_, max_items());
  if (split_ > 0) {
    CHECK_LT(t1hand_, split_);
  } else {
    CHECK_EQ(t1hand_, 0U);
  }
  if (split_ < vec_.size()) {
    CHECK_LT(t2hand_, max_items() - split_);
  } else {
    CHECK_EQ(t2hand_, 0U);
  }
  CHECK_LE(p_, max_items());
  CHECK_LE(q_, 2 * max_items());
  CHECK_LE(nn_, split_);
  CHECK_LE(ns_ + nl_, max_items());
  CHECK_EQ(nn_ + ns_ + nl_, max_items());

  const auto tb1 = t1_size() + b1_.size();
  const auto tb2 = t2_size() + b2_.size();
  CHECK_LE(tb2, max_items());
  CHECK_LE(tb1, 2 * max_items());
  CHECK_LE(tb1 + tb2, 2 * max_items());

  std::size_t nn = 0, ns = 0, nl = 0;
  for (std::size_t i = 0, n = vec_.size(); i < n; ++i) {
    const ItemPtr& slot = vec_[i];
    if (i >= split_) {
      CHECK_NOTNULL(slot.get());
      CHECK(slot->longterm);
    }
    if (!slot) {
      ++nn;
      continue;
    }
    CHECK(!slot->dead);
    if (slot->longterm)
      ++nl;
    else
      ++ns;
  }
  CHECK_EQ(nn, nn_);
  CHECK_EQ(ns, ns_);
  CHECK_EQ(nl, nl_);

  for (auto it = b1_.begin(), end = b1_.end(); it != end; ++it) {
    const ItemPtr& slot = *it;
    CHECK_NOTNULL(slot.get());
    CHECK(slot->dead);
    CHECK(!slot->used);
    CHECK(!slot->longterm);
  }
  for (auto it = b2_.begin(), end = b2_.end(); it != end; ++it) {
    const ItemPtr& slot = *it;
    CHECK_NOTNULL(slot.get());
    CHECK(slot->dead);
    CHECK(!slot->used);
    CHECK(slot->longterm);
  }
#endif
}

void CART::move_t1_index_to_t1_tail(std::size_t i) noexcept {
  // Let F denote t1hand_,
  //     I denote i.
  //
  //    BEFORE          AFTER
  //
  //    0[1 2 3]4|...   1 2 3 0 4|...
  //    ^       ^             ^ ^
  //    I       F             * F
  //
  //    0 1[2 3]4|...   0 2 3 1 4|...
  //      ^     ^             ^ ^
  //      I     F             * F
  //
  //    0 1 2[3]4|...   0 1 3 2 4|...
  //        ^   ^             ^ ^
  //        I   F             * F
  //
  //    0[1]2 3 4|...   0 2 1 3 4|...
  //      ^ ^             ^ ^
  //      F I             * F
  //
  //    0[1 2]3 4|...   0 3 1 2 4|...
  //      ^   ^           ^ ^
  //      F   I           * F
  //
  //    0[1 2 3]4|...   0 4 1 2 3|...
  //      ^     ^         ^ ^
  //      F     I         * F
  //
  //   [0]1 2 3 4|...   1 0 2 3 4|...
  //    ^ ^             ^ ^
  //    F I             * F
  //
  //   [0 1]2 3 4|...   2 0 1 3 4|...
  //    ^   ^           ^ ^
  //    F   I           * F
  //
  //   [0 1 2]3 4|...   3 0 1 2 4|...
  //    ^     ^         ^ ^
  //    F     I         * F
  //

  DCHECK_GT(split_, 0U);
  DCHECK_LT(i, split_);
  if (i < t1hand_) {
    if (i == t1hand_ - 1) return;
    auto pp = vec_.data() + i;
    auto p = pp + 1;
    auto q = vec_.data() + t1hand_;
    auto qq = q - 1;
    ItemPtr tmp = std::move(*pp);
    std::move(p, q, pp);
    *qq = std::move(tmp);
  } else if (t1hand_ == i) {
    t1_advance();
  } else {
    if (t1hand_ == 0 && i == split_ - 1) return;
    auto p = vec_.data() + t1hand_;
    auto q = vec_.data() + i;
    auto qq = q + 1;
    auto tmp = std::move(*q);
    std::move_backward(p, q, qq);
    *p = std::move(tmp);
    t1_advance();
  }
}

void CART::move_t2_index_to_t1_tail(std::size_t i) noexcept {
  // Let F denote t1hand_,
  //     G denote split_,
  //     H denote split_ + t2hand_,
  //     I denote i.
  //
  // Without loss of generality, assume F = 0.
  //
  //    BEFORE          AFTER
  //
  //    0 1|2 3 4 5     0 1 2|3 4 5     split_: 2 -> 3
  //    ^  |^           ^   ^|^         t2hand_: 0 (unchanged)
  //    F  |GHI         F   *|GH
  //
  //    0 1|2 3 4 5     0 1 2|3 4 5     split_: 2 -> 3
  //    ^  |^     ^     ^   ^|^   ^     t2hand_: 3 -> 2
  //    F  |GI    H     F   *|G   H
  //
  //    0 1|2 3 4 5     0 1 5|2 3 4     split_: 2 -> 3
  //    ^  |^     ^     ^   ^|^         t2hand_: 0 (unchanged)
  //    F  |GH    I     F   *|GH
  //
  //    0 1|2 3 4 5     0 1 5|2 3 4     split_: 2 -> 3
  //    ^  |^     ^     ^   ^|^         t2hand_: 3 -> 0 (wrapped mod 3)
  //    F  |G     HI    F   *|GH
  //
  // Observations:
  // - We need to wrap t2hand_ mod split_ after incrementing split_.
  // - We need to decrement t2hand_ iff H > I.
  //

  DCHECK_LT(split_, max_items());
  DCHECK_GE(i, split_);
  DCHECK_LT(i, max_items());

  ItemPtr *p, *q;
  if (t1hand_ == 0) {
    p = vec_.data() + split_;
  } else {
    p = vec_.data() + t1hand_;
    ++t1hand_;
  }
  q = vec_.data() + i;
  if (p != q) {
    ItemPtr tmp = std::move(*q);
    std::move_backward(p, q, q + 1);
    *p = std::move(tmp);
  }
  if (t2hand_ > i - split_) --t2hand_;
  ++split_;
  t2_wrap();
}

void CART::move_dead_to_t1_tail(ItemPtr incoming) noexcept {
  DCHECK_GT(nn_, 0U);

  std::size_t i = t1hand_;
  while (i > 0) {
    --i;
    if (vec_[i] == nullptr) {
      vec_[i] = std::move(incoming);
      move_t1_index_to_t1_tail(i);
      return;
    }
  }

  i = split_;
  while (i > t1hand_) {
    --i;
    if (vec_[i] == nullptr) {
      vec_[i] = std::move(incoming);
      move_t1_index_to_t1_tail(i);
      return;
    }
  }

  LOG(DFATAL) << "BUG! Found no nullptr values even though nn_ > 0";
}

void CART::move_t2_head_to_t1_tail() noexcept {
  std::size_t i = split_ + t2hand_;
  t2_advance();
  move_t2_index_to_t1_tail(i);
}

void CART::move_t1_head_to_t2_tail() noexcept {
  // Let F denote t1hand_,
  //     G denote split_,
  //     H denote split_ + t2hand_.
  //
  //    BEFORE          DURING            AFTER
  //
  //    0 1 2|3 4 5     0[1 2]3 4 5       1 2|0 3 4 5
  //    ^    |^         ^ ^   ^           ^  |^ ^
  //    F    |GH        * p   q           F  |G H
  //
  //    0 1 2|3 4 5     0[1 2 3]4 5       1 2|3 0 4 5
  //    ^    |^ ^       ^ ^     ^         ^  |^   ^
  //    F    |G H       * p     q         F  |G   H
  //
  //    0 1 2|3 4 5     0[1 2 3 4]5       1 2|3 4 0 5
  //    ^    |^   ^     ^ ^       ^       ^  |^     ^
  //    F    |G   H     * p       q       F  |G     H
  //
  //    0 1 2|3 4 5     0 1[2]3 4 5       0 2|1 3 4 5
  //      ^  |^           ^ ^ ^             ^|^ ^
  //      F  |GH          * p q             F|G H
  //
  //    0 1 2|3 4 5     0 1[2 3]4 5       0 2|3 1 4 5
  //      ^  |^ ^         ^ ^   ^           ^|^   ^
  //      F  |G H         * p   q           F|G   H
  //
  //    0 1 2|3 4 5     0 1[2 3 4]5       0 2|3 4 1 5
  //      ^  |^   ^       ^ ^     ^         ^|^     ^
  //      F  |G   H       * p     q         F|G     H
  //
  //    0 1 2|3 4 5     0 1 2!3 4 5       0 1|2 3 4 5
  //        ^|^             ^ ^           ^  |^ ^
  //        F|GH            * pq          F  |G H
  //
  //    0 1 2|3 4 5     0 1 2[3]4 5       0 1|3 2 4 5
  //        ^|^ ^           ^ ^ ^         ^  |^   ^
  //        F|G H           * p q         F  |G   H
  //
  //    0 1 2|3 4 5     0 1 2[3 4]5       0 1|3 4 2 5
  //        ^|^   ^         ^ ^   ^       ^  |^     ^
  //        F|G   H         * p   q       F  |G     H
  //

  auto p = vec_.data() + t1hand_ + 1;
  auto q = vec_.data() + split_ + t2hand_;
  auto pp = p - 1;
  auto qq = q - 1;
  if (p != q) {
    ItemPtr tmp = std::move(*pp);
    DCHECK(tmp && tmp->longterm);
    std::move(p, q, pp);
    *qq = std::move(tmp);
  }
  if (split_ < max_items()) ++t2hand_;
  --split_;
  t1_wrap();
}

// }}}

}  // inline namespace implementation

void append_to(std::string* out, CacheType type) {
  switch (type) {
    case CacheType::clock:
      out->append("clock");
      return;

    case CacheType::lru:
      out->append("lru");
      return;

    case CacheType::cart:
      out->append("cart");
      return;

    case CacheType::best_available:
      out->append("best_available");
      return;
  }
  LOG(DFATAL) << "BUG! Unknown CacheType " << uint16_t(type);
  out->append("unknown");
}

std::ostream& operator<<(std::ostream& o, CacheType type) {
  std::string str;
  append_to(&str, type);
  return (o << str);
}

base::Result Cache::get(std::string* out, base::StringPiece key,
                        const base::Options& opts) {
  event::Task task;
  get(&task, out, key, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result Cache::put(base::StringPiece key, base::StringPiece value,
                        const base::Options& opts) {
  event::Task task;
  put(&task, key, value, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result Cache::remove(base::StringPiece key, const base::Options& opts) {
  event::Task task;
  remove(&task, key, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result Cache::stats(CacheStats* out, const base::Options& opts) {
  event::Task task;
  stats(&task, out, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result Cache::clear(const base::Options& opts) {
  event::Task task;
  clear(&task, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

base::Result Cache::visualize(std::string* out,
                              const base::Options& opts) const {
  event::Task task;
  visualize(&task, out, opts);
  event::wait(io::get_manager(opts), &task);
  return task.result();
}

CachePtr new_cache(const CacheOptions& co) {
  switch (co.type) {
    case CacheType::clock:
      return std::make_shared<Clock>(co.max_items, co.max_bytes);

    case CacheType::lru:
      return std::make_shared<LRU>(co.max_items, co.max_bytes);

    case CacheType::cart:
    case CacheType::best_available:
      return std::make_shared<CART>(co.max_items, co.max_bytes);
  }
  LOG(DFATAL) << "BUG! Unknown CacheType " << uint16_t(co.type);
  return std::make_shared<Clock>(co.max_items, co.max_bytes);
}

}  // namespace container
