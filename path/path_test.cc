// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "gtest/gtest.h"

#include <vector>

#include "base/concat.h"
#include "path/path.h"

using Pair = std::pair<std::string, std::string>;
using Vec = std::vector<std::string>;

static Pair P(std::string a, std::string b) {
  return std::make_pair(std::move(a), std::move(b));
}

static std::string stringify(const Vec& vec) {
  std::string out;
  out.push_back('[');

  bool first = true;
  for (const auto& item : vec) {
    if (first)
      first = false;
    else
      out.push_back(' ');
    out.push_back('"');
    out.append(item);
    out.push_back('"');
  }
  out.push_back(']');
  return out;
}

static testing::AssertionResult same_vector(const char* lhs_expr,
                                            const char* rhs_expr,
                                            const Vec& lhs,
                                            const Vec& rhs) {
  bool eq = (lhs.size() == rhs.size());
  if (eq) {
    for (std::size_t i = 0, n = lhs.size(); i < n; ++i) {
      if (lhs[i] != rhs[i]) {
        eq = false;
        break;
      }
    }
  }
  if (eq) return testing::AssertionSuccess();
  return testing::AssertionFailure() << lhs_expr << " differs from " << rhs_expr
                                     << "\n"
                                     << "expected: " << stringify(lhs) << "\n"
                                     << "  actual: " << stringify(rhs);
}

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

TEST(Path, Explode) {
  struct TestItem {
    std::string path;
    std::vector<std::string> expected;
  };

  std::vector<TestItem> testdata{
      {"", {"."}},
      {".", {"."}},
      {"..", {".."}},
      {"foo", {"foo"}},
      {"./foo", {".", "foo"}},
      {"../foo", {"..", "foo"}},
      {"foo/.", {"foo", "."}},
      {"foo/..", {"foo", ".."}},
      {"foo/bar", {"foo", "bar"}},

      {"/", {"/"}},
      {"/.", {"/", "."}},
      {"/..", {"/", ".."}},
      {"/foo", {"/", "foo"}},
      {"/./foo", {"/", ".", "foo"}},
      {"/../foo", {"/", "..", "foo"}},
      {"/foo/.", {"/", "foo", "."}},
      {"/foo/..", {"/", "foo", ".."}},
      {"/foo/bar", {"/", "foo", "bar"}},

      // Trailing slash {{{
      {"./", {"."}},
      {"../", {".."}},
      {"foo/", {"foo"}},
      {"./foo/", {".", "foo"}},
      {"../foo/", {"..", "foo"}},
      {"foo/./", {"foo", "."}},
      {"foo/../", {"foo", ".."}},
      {"foo/bar/", {"foo", "bar"}},

      {"/./", {"/", "."}},
      {"/../", {"/", ".."}},
      {"/foo/", {"/", "foo"}},
      {"/./foo/", {"/", ".", "foo"}},
      {"/../foo/", {"/", "..", "foo"}},
      {"/foo/./", {"/", "foo", "."}},
      {"/foo/../", {"/", "foo", ".."}},
      {"/foo/bar/", {"/", "foo", "bar"}},
      // }}}

      // Doubled slashes {{{
      {".//foo", {".", "foo"}},
      {"..//foo", {"..", "foo"}},
      {"foo//.", {"foo", "."}},
      {"foo//..", {"foo", ".."}},
      {"foo//bar", {"foo", "bar"}},

      {"//", {"/"}},
      {"//.", {"/", "."}},
      {"//..", {"/", ".."}},
      {"//foo", {"/", "foo"}},
      {"//.//foo", {"/", ".", "foo"}},
      {"//..//foo", {"/", "..", "foo"}},
      {"//foo//.", {"/", "foo", "."}},
      {"//foo//..", {"/", "foo", ".."}},
      {"//foo//bar", {"/", "foo", "bar"}},

      // Trailing slash {{{
      {".//", {"."}},
      {"..//", {".."}},
      {"foo//", {"foo"}},
      {".//foo//", {".", "foo"}},
      {"..//foo//", {"..", "foo"}},
      {"foo//.//", {"foo", "."}},
      {"foo//..//", {"foo", ".."}},
      {"foo//bar//", {"foo", "bar"}},

      {"//.//", {"/", "."}},
      {"//..//", {"/", ".."}},
      {"//foo//", {"/", "foo"}},
      {"//.//foo//", {"/", ".", "foo"}},
      {"//..//foo//", {"/", "..", "foo"}},
      {"//foo//.//", {"/", "foo", "."}},
      {"//foo//..//", {"/", "foo", ".."}},
      {"//foo//bar//", {"/", "foo", "bar"}},
      // }}}
      // }}}
  };

  for (const auto& row : testdata) {
    SCOPED_TRACE(row.path);
    EXPECT_PRED_FORMAT2(same_vector, row.expected, path::explode(row.path));
  }
}

TEST(Path, Split) {
  // Test cases are taken from dirname(1) + basename(1), like so:
  //
  //   testcases=(
  //     ""
  //     .{,/}
  //     ..{,/}
  //     foo{,/,/bar}
  //     ./foo{,/,/bar}
  //     ../foo{,/,/bar}
  //     /
  //     /foo{,/,/bar}
  //   )
  //   for x in "${testcases[@]}"; do
  //     d="$(dirname "$x")"
  //     b="$(basename "$x")"
  //     printf '[%10s] [%10s] [%10s]\n' "$x" "$d" "$b"
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

TEST(Path, AbsPath) {
  struct TestItem {
    std::string root;
    std::string path;
    std::string expected;
  };

  std::vector<TestItem> testdata{
      {"/", "", "/"},
      {"/", ".", "/"},
      {"/", "..", "/"},
      {"/", "foo", "/foo"},
      {"/", "./foo", "/foo"},
      {"/", "../foo", "/foo"},
      {"/", "/", "/"},
      {"/", "/foo", "/foo"},

      {"/foo/bar", "", "/foo/bar"},
      {"/foo/bar", ".", "/foo/bar"},
      {"/foo/bar", "..", "/foo"},
      {"/foo/bar", "baz", "/foo/bar/baz"},
      {"/foo/bar", "./baz", "/foo/bar/baz"},
      {"/foo/bar", "../baz", "/foo/baz"},
      {"/foo/bar", "/", "/"},
      {"/foo/bar", "/baz", "/baz"},
  };

  for (const auto& row : testdata) {
    SCOPED_TRACE(base::concat("root=", row.root, " path=", row.path));
    EXPECT_EQ(row.expected, path::abspath(row.path, row.root));
  }
}

TEST(Path, RelPath) {
  struct TestItem {
    std::string root;
    std::string path;
    std::string expected;
  };

  std::vector<TestItem> testdata{
      {"/", "", "."},
      {"/", ".", "."},
      {"/", "..", ".."},
      {"/", "foo", "foo"},
      {"/", "./foo", "foo"},
      {"/", "../foo", "../foo"},

      {"/foo/bar", "", "."},
      {"/foo/bar", ".", "."},
      {"/foo/bar", "..", ".."},
      {"/foo/bar", "baz", "baz"},
      {"/foo/bar", "./baz", "baz"},
      {"/foo/bar", "../baz", "../baz"},

      {"/", "/", "."},
      {"/", "/foo", "foo"},

      {"/foo", "/", ".."},
      {"/foo", "/foo", "."},
      {"/foo", "/foo/bar", "bar"},
      {"/foo", "/bar", "../bar"},

      {"/foo/bar", "/", "../.."},
      {"/foo/bar", "/foo", ".."},
      {"/foo/bar", "/foo/bar", "."},
      {"/foo/bar", "/foo/bar/baz", "baz"},
      {"/foo/bar", "/foo/baz", "../baz"},
      {"/foo/bar", "/baz", "../../baz"},

      {"/foo", "/foobar", "../foobar"},
      {"/foobar", "/foobaz", "../foobaz"},
  };

  for (const auto& row : testdata) {
    SCOPED_TRACE(base::concat("root=", row.root, " path=", row.path));
    EXPECT_EQ(row.expected, path::relpath(row.path, row.root));
  }
}
