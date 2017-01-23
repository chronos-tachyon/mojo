# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "container",
  srcs = [
    "cache.cc",
  ],
  hdrs = [
    "cache.h",
  ],
  deps = [
    "//base",
    "//event",
    "//io",
  ],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "cache_test",
  srcs = ["cache_test.cc"],
  deps = [
    ":container",
    "//base:result_testing",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)
