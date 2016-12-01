# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "backport",
  srcs = ["backport.cc"],
  hdrs = ["backport.h"],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "cleanup",
  srcs = ["cleanup.cc"],
  hdrs = ["cleanup.h"],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "cleanup_test",
  srcs = ["cleanup_test.cc"],
  deps = [
    ":cleanup",
    "//external:gtest",
  ],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "concat",
  srcs = ["concat.cc"],
  hdrs = ["concat.h"],
  deps = [":backport"],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "concat_test",
  srcs = ["concat_test.cc"],
  deps = [
    ":concat",
    "//external:gtest",
  ],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "debug",
  srcs = ["debug.cc"],
  hdrs = ["debug.h"],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "logging",
  srcs = ["logging.cc"],
  hdrs = ["logging.h"],
  deps = [":debug"],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "logging_test",
  srcs = ["logging_test.cc"],
  deps = [
    ":logging",
    "//external:gtest",
  ],
)

cc_library(
  name = "result",
  srcs = ["result.cc"],
  hdrs = ["result.h"],
  deps = [
    ":concat",
    ":logging",
  ],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "result_test",
  srcs = ["result_test.cc"],
  deps = [
    ":result",
    "//external:gtest",
  ],
)

cc_library(
  name = "result_testing",
  srcs = ["result_testing.cc"],
  hdrs = ["result_testing.h"],
  deps = [
    ":result",
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

cc_library(
  name = "time",
  srcs = [
    "clock.cc",
    "duration.cc",
    "stopwatch.cc",
    "time.cc",
  ],
  hdrs = [
    "clock.h",
    "duration.h",
    "stopwatch.h",
    "time.h",
  ],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "clock_test",
  srcs = ["clock_test.cc"],
  deps = [
    ":time",
    "//external:gtest",
  ],
)

cc_test(
  name = "duration_test",
  srcs = ["duration_test.cc"],
  deps = [
    ":time",
    "//external:gtest",
  ],
)

cc_test(
  name = "stopwatch_test",
  srcs = ["stopwatch_test.cc"],
  deps = [
    ":time",
    "//external:gtest",
  ],
)

cc_test(
  name = "time_test",
  srcs = ["time_test.cc"],
  deps = [
    ":time",
    "//external:gtest",
  ],
)

cc_library(
  name = "token",
  srcs = ["token.cc"],
  hdrs = ["token.h"],
  visibility = ["//visibility:public"],
)
