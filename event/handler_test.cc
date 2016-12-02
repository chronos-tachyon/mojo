// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <ostream>

#include "base/result_testing.h"
#include "event/handler.h"

struct Count {
  int num;
  int r;
  int w;
  int p;
  int h;
  int e;

  Count(int num, int r, int w, int p, int h, int e) noexcept : num(num),
                                                               r(r),
                                                               w(w),
                                                               p(p),
                                                               h(h),
                                                               e(e) {}
  Count() noexcept : Count(0, 0, 0, 0, 0, 0) {}

  void bump(event::Set set) {
    ++num;
    if (set.readable()) ++r;
    if (set.writable()) ++w;
    if (set.priority()) ++p;
    if (set.hangup()) ++h;
    if (set.error()) ++e;
  }
};

inline bool operator==(const Count& a, const Count& b) noexcept {
  return a.num == b.num && a.r == b.r && a.w == b.w && a.p == b.p &&
         a.h == b.h && a.e == b.e;
}

inline std::ostream& operator<<(std::ostream& os, const Count& c) {
  os << "Count(num=" << c.num << ", r=" << c.r << ", w=" << c.w << ", p=" << c.p
     << ", h=" << c.h << ", e=" << c.e << ")";
  return os;
}

struct Immobile {
  int dummy;

  Immobile() : dummy(1) {}
  Immobile(const Immobile&) = delete;
  Immobile(Immobile&&) = delete;
  Immobile& operator=(const Immobile&) = delete;
  Immobile& operator=(Immobile&&) = delete;
};

TEST(Handler, Basics) {
  Count c;
  auto closure = [&c](const std::unique_ptr<Immobile>& ptr, event::Data data) {
    c.bump(data.events);
    ptr->dummy *= 2;
    if (ptr->dummy >= 8)
      return base::Result::out_of_range("my spoon is too big");
    return base::Result();
  };

  std::unique_ptr<Immobile> ptr(new Immobile);
  int* dummy_ptr = &ptr->dummy;
  auto h = event::handler(closure, std::move(ptr));
  EXPECT_EQ(Count(), c);
  EXPECT_EQ(1, *dummy_ptr);

  event::Data data;

  data.events = event::Set();
  EXPECT_OK(h->run(data));
  EXPECT_EQ(Count(1, 0, 0, 0, 0, 0), c);
  EXPECT_EQ(2, *dummy_ptr);

  data.events = event::Set::priority_bit();
  EXPECT_OK(h->run(data));
  EXPECT_EQ(Count(2, 0, 0, 1, 0, 0), c);
  EXPECT_EQ(4, *dummy_ptr);

  data.events = event::Set::all_bits();
  EXPECT_OUT_OF_RANGE(h->run(data));
  EXPECT_EQ(Count(3, 1, 1, 2, 1, 1), c);
  EXPECT_EQ(8, *dummy_ptr);
}
