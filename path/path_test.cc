// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include "path/path.h"

TEST(Path, CleanRooted) {
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

TEST(Path, CleanRelative) {
  EXPECT_EQ(".", path::clean(""));
  EXPECT_EQ(".", path::clean("."));
  EXPECT_EQ(".", path::clean("./"));
  EXPECT_EQ(".", path::clean(".//"));

  EXPECT_EQ("foo", path::clean("foo"));
  EXPECT_EQ("foo", path::clean("foo/"));
  EXPECT_EQ("foo", path::clean("foo//"));
  EXPECT_EQ("foo", path::clean("./foo"));
  EXPECT_EQ("foo", path::clean(".//foo"));
  EXPECT_EQ("foo", path::clean("./foo/"));
  EXPECT_EQ("foo", path::clean(".//foo//"));

  EXPECT_EQ("foo/bar", path::clean("foo/bar"));
  EXPECT_EQ("foo/bar", path::clean("foo//bar"));
  EXPECT_EQ("foo/bar", path::clean("foo/bar/"));
  EXPECT_EQ("foo/bar", path::clean("foo//bar//"));
  EXPECT_EQ("foo/bar", path::clean("./foo/bar"));
  EXPECT_EQ("foo/bar", path::clean(".//foo//bar"));
  EXPECT_EQ("foo/bar", path::clean("./foo/bar/"));
  EXPECT_EQ("foo/bar", path::clean(".//foo//bar//"));

  EXPECT_EQ("..", path::clean(".."));
  EXPECT_EQ("..", path::clean("../"));
  EXPECT_EQ("..", path::clean("..//"));

  EXPECT_EQ("../foo", path::clean("../foo"));
  EXPECT_EQ("../foo", path::clean("..//foo"));
  EXPECT_EQ("../foo", path::clean("../foo/"));
  EXPECT_EQ("../foo", path::clean("..//foo//"));

  EXPECT_EQ("..", path::clean("../foo/.."));
  EXPECT_EQ("..", path::clean("..//foo//.."));
  EXPECT_EQ("..", path::clean("../foo/../"));
  EXPECT_EQ("..", path::clean("..//foo//..//"));

  EXPECT_EQ("../bar", path::clean("../foo/../bar"));
  EXPECT_EQ("../bar", path::clean("..//foo//..//bar"));
  EXPECT_EQ("../bar", path::clean("../foo/../bar/"));
  EXPECT_EQ("../bar", path::clean("..//foo//..//bar//"));
}

using Pair = std::pair<std::string, std::string>;

static Pair P(std::string a, std::string b) {
  return std::make_pair(std::move(a), std::move(b));
}

TEST(Path, split) {
  EXPECT_EQ(P("/", ""), path::split("/"));
  EXPECT_EQ(P("/", "foo"), path::split("/foo"));
  EXPECT_EQ(P("/foo", "bar"), path::split("/foo/bar"));

  EXPECT_EQ(P(".", ""), path::split(""));
  EXPECT_EQ(P(".", ""), path::split("."));
  EXPECT_EQ(P(".", "foo"), path::split("foo"));
  EXPECT_EQ(P("foo", "bar"), path::split("foo/bar"));
  EXPECT_EQ(P("..", ""), path::split(".."));
  EXPECT_EQ(P("..", "foo"), path::split("../foo"));
}
