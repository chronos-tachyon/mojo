# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "time",
  srcs = [
    "breakdown.cc",
    "clock.cc",
    "duration.cc",
    "stopwatch.cc",
    "time.cc",
    "zone.cc",
    "zone_posix.cc",
    "zone_tzif.cc",
  ],
  hdrs = [
    "breakdown.h",
    "clock.h",
    "duration.h",
    "stopwatch.h",
    "time.h",
    "zone.h",
    "zone_posix.h",
    "zone_tzif.h",
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
  name = "breakdown_test",
  srcs = ["breakdown_test.cc"],
  deps = [
    ":time",
    "//base:result_testing",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
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

cc_test(
  name = "zone_test",
  srcs = ["zone_test.cc"],
  deps = [
    ":time",
    "//base:result_testing",
    "//external:gtest",
  ],
  data = glob(["testdata/*"]),
  size = "small",
  timeout = "short",
)

cc_binary(
  name = "tzdump",
  srcs = ["tzdump.cc"],
  deps = [":time"],
)

cc_binary(
  name = "tzlist",
  srcs = ["tzlist.cc"],
  deps = [":time"],
)
