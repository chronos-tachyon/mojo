// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include <string>

#include "base/options.h"
#include "gtest/gtest.h"

struct A : public base::OptionsType {
  int foo;
  bool bar;

  A() noexcept : foo(42), bar(true) {}
};

struct B : public base::OptionsType {
  std::string baz;

  B() noexcept : baz("23") {}
};

int get_foo(const A& a) { return a.foo; }

bool get_bar(const A& a) { return a.bar; }

std::string get_baz(const B& b) { return b.baz; }

TEST(Options, Basics) {
  base::Options o;
  EXPECT_EQ(42, get_foo(o));
  EXPECT_TRUE(get_bar(o));
  EXPECT_EQ("23", get_baz(o));

  o.get<A>().foo++;
  o.get<A>().bar = false;
  o.get<B>().baz = "5";

  EXPECT_EQ(43, get_foo(o));
  EXPECT_FALSE(get_bar(o));
  EXPECT_EQ("5", get_baz(o));
}
