# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "url",
  srcs = [
    "_internal.h",
    "_internal.cc",
    "query.cc",
    "url.cc",
  ],
  hdrs = [
    "query.h",
    "url.h",
  ],
  deps = ["//base"],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "query_test",
  srcs = ["query_test.cc"],
  deps = [
    ":url",
    "//base:result_testing",
    "//external:gtest",
  ],
  timeout = "short",
  size = "small",
)

cc_test(
  name = "url_test",
  srcs = ["url_test.cc"],
  deps = [
    ":url",
    "//base:result_testing",
    "//external:gtest",
  ],
  timeout = "short",
  size = "small",
)
