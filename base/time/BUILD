# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

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
  deps = ["//base"],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "clockfake",
  srcs = ["clockfake.cc"],
  hdrs = ["clockfake.h"],
  deps = [":time"],
  visibility = ["//visibility:public"],
  testonly = True,
)

cc_test(
  name = "clock_test",
  srcs = ["clock_test.cc"],
  deps = [
    ":time",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "duration_test",
  srcs = ["duration_test.cc"],
  deps = [
    ":time",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "stopwatch_test",
  srcs = ["stopwatch_test.cc"],
  deps = [
    ":time",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "time_test",
  srcs = ["time_test.cc"],
  deps = [
    ":time",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)
