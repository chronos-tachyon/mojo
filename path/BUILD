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
