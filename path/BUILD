# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "path",
  srcs = [
    "clean.cc",
  ],
  hdrs = [
    "clean.h",
  ],
  deps = [
  ],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "clean_test",
  srcs = ["clean_test.cc"],
  deps = [
    ":path",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)
