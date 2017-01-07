// Copyright © 2016 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "path/clean.h"

TEST(PathClean, Rooted) {
  EXPECT_EQ("/", path::clean("/"));
  EXPECT_EQ("/", path::clean("//"));

  EXPECT_EQ("/", path::clean("/."));
  EXPECT_EQ("/", path::clean("//."));

  EXPECT_EQ("/", path::clean("/./"));
  EXPECT_EQ("/", path::clean("//.//"));

  EXPECT_EQ("/", path::clean("/.."));
  EXPECT_EQ("/", path::clean("//.."));

  EXPECT_EQ("/", path::clean("/../"));
  EXPECT_EQ("/", path::clean("//..//"));

  EXPECT_EQ("/", path::clean("/foo/.."));
  EXPECT_EQ("/", path::clean("//foo//.."));

  EXPECT_EQ("/", path::clean("/foo/../"));
  EXPECT_EQ("/", path::clean("//foo//..//"));

  EXPECT_EQ("/foo", path::clean("/./foo"));
  EXPECT_EQ("/foo", path::clean("//.//foo"));

  EXPECT_EQ("/foo", path::clean("/../foo/"));
  EXPECT_EQ("/foo", path::clean("//..//foo//"));

  EXPECT_EQ("/bar", path::clean("/foo/../bar"));
  EXPECT_EQ("/bar", path::clean("//foo//..//bar"));

  EXPECT_EQ("/bar", path::clean("/foo/./../bar"));
  EXPECT_EQ("/bar", path::clean("//foo//.//..//bar/"));
}
