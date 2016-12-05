# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "base",
  srcs = [
    "backport.cc",
    "cleanup.cc",
    "clock.cc",
    "concat.cc",
    "debug.cc",
    "duration.cc",
    "fd.cc",
    "logging.cc",
    "result.cc",
    "stopwatch.cc",
    "time.cc",
    "token.cc",
    "util.cc",
  ],
  hdrs = [
    "backport.h",
    "cleanup.h",
    "clock.h",
    "concat.h",
    "debug.h",
    "duration.h",
    "fd.h",
    "logging.h",
    "result.h",
    "stopwatch.h",
    "time.h",
    "token.h",
    "util.h",
  ],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "cleanup_test",
  srcs = ["cleanup_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "clock_test",
  srcs = ["clock_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
)

cc_test(
  name = "concat_test",
  srcs = ["concat_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "duration_test",
  srcs = ["duration_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
)

cc_test(
  name = "logging_test",
  srcs = ["logging_test.cc"],
  deps = [
    ":base",
    ":result_testing",
    "//external:gtest",
    "//external:re2",
  ],
)

cc_test(
  name = "result_test",
  srcs = ["result_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
)

cc_test(
  name = "stopwatch_test",
  srcs = ["stopwatch_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
)

cc_test(
  name = "time_test",
  srcs = ["time_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
)

cc_library(
  name = "result_testing",
  srcs = ["result_testing.cc"],
  hdrs = ["result_testing.h"],
  deps = [
    ":base",
    "//external:gtest_nomain",
  ],
  visibility = ["//visibility:public"],
  testonly = True,
)

cc_test(
  name = "result_testing_test",
  srcs = ["result_testing_test.cc"],
  deps = [
    ":result_testing",
    "//external:gtest",
  ],
)
