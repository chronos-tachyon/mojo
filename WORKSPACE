# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

workspace(name = "com_github_chronostachyon_mojo")

load(
  "@bazel_tools//tools/build_defs/repo:git.bzl",
  git_repository = "git_repository",
)

git_repository(
  name = "com_googlesource_code_re2",
  remote = "https://github.com/google/re2.git",
  tag = "2016-11-01",
)

bind(
  name = "re2",
  actual = "@com_googlesource_code_re2//:re2",
)

git_repository(
  name = "googletest",
  remote = "https://github.com/chronos-tachyon/googletest-bazel.git",
  tag = "release-1.8.0-bazel-20161117",
)

bind(
  name = "gtest",
  actual = "@googletest//:gtest_main",
)

bind(
  name = "gtest_nomain",
  actual = "@googletest//:gtest",
)
