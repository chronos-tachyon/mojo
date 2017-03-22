# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "io",
  srcs = [
    "buffer.cc",
    "chain.cc",
    "common.cc",
    "options.cc",
    "pipe.cc",
    "ratelimiter.cc",
    "reader.cc",
    "util.cc",
    "writer.cc",
  ],
  hdrs = [
    "buffer.h",
    "chain.h",
    "common.h",
    "options.h",
    "pipe.h",
    "ratelimiter.h",
    "reader.h",
    "util.h",
    "writer.h",
  ],
  deps = [
    "//base",
    "//base/time",
    "//event",
  ],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "buffer_test",
  srcs = ["buffer_test.cc"],
  deps = [
    ":io",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "pipe_test",
  srcs = ["pipe_test.cc"],
  deps = [
    ":io",
    "//base:result_testing",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "reader_test",
  srcs = ["reader_test.cc"],
  deps = [
    ":io",
    "//base:result_testing",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "util_test",
  srcs = ["util_test.cc"],
  deps = [
    ":io",
    "//base:result_testing",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "writer_test",
  srcs = ["writer_test.cc"],
  deps = [
    ":io",
    "//base:result_testing",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_library(
  name = "testing",
  srcs = ["testing.cc"],
  hdrs = ["testing.h"],
  deps = [":io"],
  visibility = ["//visibility:public"],
  testonly = True,
)

cc_test(
  name = "testing_test",
  srcs = ["testing_test.cc"],
  deps = [
    ":testing",
    "//base:result_testing",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)
