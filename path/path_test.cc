// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <vector>

#include "path/path.h"

TEST(Path, Clean) {
  struct TestItem {
    std::string input;
    std::string expected_partial;
    std::string expected_full;
  };

  std::vector<TestItem> testdata{
      // Relative {{{
      {"", ".", "."},

      {".", ".", "."},
      {"..", "..", ".."},
      {"foo", "foo", "foo"},
      {"foo/.", "foo", "foo"},
      {"foo/..", "foo/..", "."},
      {"foo/bar", "foo/bar", "foo/bar"},
      {"foo/./bar", "foo/bar", "foo/bar"},
      {"foo/../bar", "foo/../bar", "bar"},
      {"./foo", "foo", "foo"},
      {"../foo", "../foo", "../foo"},
      {"../foo/..", "../foo/..", ".."},
      {"../foo/../bar", "../foo/../bar", "../bar"},

      // Trailing slashes {{{
      {"./", ".", "."},
      {"../", "..", ".."},
      {"foo/", "foo", "foo"},
      {"foo/./", "foo", "foo"},
      {"foo/../", "foo/..", "."},
      {"foo/bar/", "foo/bar", "foo/bar"},
      {"foo/./bar/", "foo/bar", "foo/bar"},
      {"foo/../bar/", "foo/../bar", "bar"},
      {"./foo/", "foo", "foo"},
      {"../foo/", "../foo", "../foo"},
      {"../foo/../", "../foo/..", ".."},
      {"../foo/../bar/", "../foo/../bar", "../bar"},
      // }}}
      // }}}

      // Absolute {{{
      {"/", "/", "/"},

      {"/.", "/", "/"},
      {"/..", "/..", "/"},
      {"/foo", "/foo", "/foo"},
      {"/foo/.", "/foo", "/foo"},
      {"/foo/./bar", "/foo/bar", "/foo/bar"},
      {"/foo/..", "/foo/..", "/"},
      {"/foo/../bar", "/foo/../bar", "/bar"},
      {"/foo/./..", "/foo/..", "/"},
      {"/foo/./../bar", "/foo/../bar", "/bar"},
      {"/./foo", "/foo", "/foo"},
      {"/../foo", "/../foo", "/foo"},

      // Trailing slashes {{{
      {"/./", "/", "/"},
      {"/../", "/..", "/"},
      {"/foo/", "/foo", "/foo"},
      {"/foo/./", "/foo", "/foo"},
      {"/foo/./bar/", "/foo/bar", "/foo/bar"},
      {"/foo/../", "/foo/..", "/"},
      {"/foo/../bar/", "/foo/../bar", "/bar"},
      {"/foo/./../", "/foo/..", "/"},
      {"/foo/./../bar/", "/foo/../bar", "/bar"},
      {"/./foo/", "/foo", "/foo"},
      {"/../foo/", "/../foo", "/foo"},
      // }}}
      // }}}

      // Doubled slashes {{{

      // Relative {{{

      {"foo//.", "foo", "foo"},
      {"foo//..", "foo/..", "."},
      {"foo//bar", "foo/bar", "foo/bar"},
      {"foo//.//bar", "foo/bar", "foo/bar"},
      {"foo//..//bar", "foo/../bar", "bar"},
      {".//foo", "foo", "foo"},
      {"..//foo", "../foo", "../foo"},
      {"..//foo//..", "../foo/..", ".."},
      {"..//foo//..//bar", "../foo/../bar", "../bar"},

      // Trailing slashes {{{
      {".//", ".", "."},
      {"..//", "..", ".."},
      {"foo//", "foo", "foo"},
      {"foo//.//", "foo", "foo"},
      {"foo//..//", "foo/..", "."},
      {"foo//bar//", "foo/bar", "foo/bar"},
      {"foo//.//bar//", "foo/bar", "foo/bar"},
      {"foo//..//bar//", "foo/../bar", "bar"},
      {".//foo//", "foo", "foo"},
      {"..//foo//", "../foo", "../foo"},
      {"..//foo//..//", "../foo/..", ".."},
      {"..//foo//..//bar//", "../foo/../bar", "../bar"},
      // }}}
      // }}}

      // Absolute {{{
      {"//", "/", "/"},

      {"//.", "/", "/"},
      {"//..", "/..", "/"},
      {"//foo", "/foo", "/foo"},
      {"//foo//.", "/foo", "/foo"},
      {"//foo//.//bar", "/foo/bar", "/foo/bar"},
      {"//foo//..", "/foo/..", "/"},
      {"//foo//..//bar", "/foo/../bar", "/bar"},
      {"//foo//.//..", "/foo/..", "/"},
      {"//foo//.//..//bar", "/foo/../bar", "/bar"},
      {"//.//foo", "/foo", "/foo"},
      {"//..//foo", "/../foo", "/foo"},

      // Trailing slashes {{{
      {"//.//", "/", "/"},
      {"//..//", "/..", "/"},
      {"//foo//", "/foo", "/foo"},
      {"//foo//.//", "/foo", "/foo"},
      {"//foo//.//bar//", "/foo/bar", "/foo/bar"},
      {"//foo//..//", "/foo/..", "/"},
      {"//foo//..//bar//", "/foo/../bar", "/bar"},
      {"//foo//.//..//", "/foo/..", "/"},
      {"//foo//.//..//bar//", "/foo/../bar", "/bar"},
      {"//.//foo//", "/foo", "/foo"},
      {"//..//foo//", "/../foo", "/foo"},
      // }}}
      // }}}

      // }}}
  };

  for (const auto& row : testdata) {
    SCOPED_TRACE(row.input);
    EXPECT_EQ(row.expected_partial, path::partial_clean(row.input));
    EXPECT_EQ(row.expected_full, path::clean(row.input));
  }
}

using Pair = std::pair<std::string, std::string>;

static Pair P(std::string a, std::string b) {
  return std::make_pair(std::move(a), std::move(b));
}

TEST(Path, Split) {
  // Test cases are taken from:
  //
  //   for x in "" .{,/} ..{,/} foo{,/,/bar} ./foo{,/,/bar} ../foo{,/,/bar} / /foo{,/,/bar}
  //   do
  //     printf '[%10s] [%10s] [%10s]\n' "$x" "$(dirname "$x")" "$(basename "$x")"
  //   done
  //
  // Which outputs:
  //
  //   [          ] [         .] [          ]
  //   [         .] [         .] [         .]
  //   [        ./] [         .] [         .]
  //   [        ..] [         .] [        ..]
  //   [       ../] [         .] [        ..]
  //   [       foo] [         .] [       foo]
  //   [      foo/] [         .] [       foo]
  //   [   foo/bar] [       foo] [       bar]
  //   [     ./foo] [         .] [       foo]
  //   [    ./foo/] [         .] [       foo]
  //   [ ./foo/bar] [     ./foo] [       bar]
  //   [    ../foo] [        ..] [       foo]
  //   [   ../foo/] [        ..] [       foo]
  //   [../foo/bar] [    ../foo] [       bar]
  //   [         /] [         /] [         /]
  //   [      /foo] [         /] [       foo]
  //   [     /foo/] [         /] [       foo]
  //   [  /foo/bar] [      /foo] [       bar]

  EXPECT_EQ(P(".", ""), path::split(""));

  EXPECT_EQ(P(".", "."), path::split("."));
  EXPECT_EQ(P(".", "."), path::split("./"));

  EXPECT_EQ(P(".", ".."), path::split(".."));
  EXPECT_EQ(P(".", ".."), path::split("../"));

  EXPECT_EQ(P(".", "foo"), path::split("foo"));
  EXPECT_EQ(P(".", "foo"), path::split("foo/"));
  EXPECT_EQ(P("foo", "bar"), path::split("foo/bar"));

  EXPECT_EQ(P(".", "foo"), path::split("./foo"));
  EXPECT_EQ(P(".", "foo"), path::split("./foo/"));
  EXPECT_EQ(P("./foo", "bar"), path::split("./foo/bar"));

  EXPECT_EQ(P("..", "foo"), path::split("../foo"));
  EXPECT_EQ(P("..", "foo"), path::split("../foo/"));
  EXPECT_EQ(P("../foo", "bar"), path::split("../foo/bar"));

  EXPECT_EQ(P("/", "/"), path::split("/"));
  EXPECT_EQ(P("/", "foo"), path::split("/foo"));
  EXPECT_EQ(P("/", "foo"), path::split("/foo/"));
  EXPECT_EQ(P("/foo", "bar"), path::split("/foo/bar"));
}

TEST(Path, Join) {
  EXPECT_EQ("", path::join("", ""));

  EXPECT_EQ("foo", path::join("", "foo"));
  EXPECT_EQ(".", path::join("", "."));
  EXPECT_EQ("..", path::join("", ".."));

  EXPECT_EQ("/foo", path::join("", "/foo"));
  EXPECT_EQ("/.", path::join("", "/."));
  EXPECT_EQ("/..", path::join("", "/.."));

  EXPECT_EQ("foo", path::join("foo", ""));
  EXPECT_EQ(".", path::join(".", ""));
  EXPECT_EQ("..", path::join("..", ""));

  EXPECT_EQ("foo/", path::join("foo/", ""));
  EXPECT_EQ("./", path::join("./", ""));
  EXPECT_EQ("../", path::join("../", ""));

  EXPECT_EQ("foo/bar", path::join("foo", "bar"));
  EXPECT_EQ("./.", path::join(".", "."));
  EXPECT_EQ("../..", path::join("..", ".."));

  EXPECT_EQ("foo/bar", path::join("foo/", "bar"));
  EXPECT_EQ("./.", path::join("./", "."));
  EXPECT_EQ("../..", path::join("../", ".."));

  EXPECT_EQ("foo/bar", path::join("foo", "/bar"));
  EXPECT_EQ("./.", path::join(".", "/."));
  EXPECT_EQ("../..", path::join("..", "/.."));

  EXPECT_EQ("foo/bar/baz", path::join("foo", "bar", "baz"));
}
