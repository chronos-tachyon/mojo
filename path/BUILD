# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "path",
  srcs = [
    "path.cc",
  ],
  hdrs = [
    "path.h",
  ],
  deps = ["//base"],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "path_test",
  srcs = ["path_test.cc"],
  deps = [
    ":path",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_binary(
  name = "abspath",
  srcs = ["abspath.cc"],
  deps = [
    "//base",
    ":path",
  ],
)

cc_binary(
  name = "realpath",
  srcs = ["realpath.cc"],
  deps = [
    "//base",
    ":path",
  ],
)

cc_binary(
  name = "relpath",
  srcs = ["relpath.cc"],
  deps = [
    "//base",
    ":path",
  ],
)
