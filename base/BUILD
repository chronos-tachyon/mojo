# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "base",
  srcs = [
    "backport.cc",
    "cleanup.cc",
    "clock.cc",
    "concat.cc",
    "cpu.cc",
    "debug.cc",
    "duration.cc",
    "endian.cc",
    "fd.cc",
    "int128.cc",
    "logging.cc",
    "mutex.cc",
    "options.cc",
    "result.cc",
    "safemath.cc",
    "stopwatch.cc",
    "strings.cc",
    "time.cc",
    "token.cc",
    "user.cc",
  ],
  hdrs = [
    "backport.h",
    "cleanup.h",
    "clock.h",
    "concat.h",
    "cpu.h",
    "debug.h",
    "duration.h",
    "endian.h",
    "fd.h",
    "int128.h",
    "logging.h",
    "mutex.h",
    "options.h",
    "result.h",
    "safemath.h",
    "stopwatch.h",
    "strings.h",
    "time.h",
    "token.h",
    "user.h",
  ],
  deps = [
    "//external:re2",
  ],
  linkopts = ["-pthread"],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "cleanup_test",
  srcs = ["cleanup_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "clock_test",
  srcs = ["clock_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "concat_test",
  srcs = ["concat_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "duration_test",
  srcs = ["duration_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "endian_test",
  srcs = ["endian_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "int128_test",
  srcs = ["int128_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
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
  size = "small",
  timeout = "short",
)

cc_test(
  name = "options_test",
  srcs = ["options_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "result_test",
  srcs = ["result_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "safemath_test",
  srcs = ["safemath_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "stopwatch_test",
  srcs = ["stopwatch_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "strings_test",
  srcs = ["strings_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "time_test",
  srcs = ["time_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "user_test",
  srcs = ["user_test.cc"],
  deps = [
    ":base",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_library(
  name = "clockfake",
  srcs = ["clockfake.cc"],
  hdrs = ["clockfake.h"],
  deps = [":base"],
  visibility = ["//visibility:public"],
  testonly = True,
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
  size = "small",
  timeout = "short",
)

cc_binary(
  name = "cpu_list",
  srcs = ["cpu_list.cc"],
  deps = [":base"],
)

cc_binary(
  name = "user_lookup",
  srcs = ["user_lookup.cc"],
  deps = [":base"],
)
