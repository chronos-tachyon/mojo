# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "core",
  srcs = [
    "file.cc",
    "fd.cc",
    "fs.cc",
    "mode.cc",
    "options.cc",
    "perm.cc",
    "registry.cc",
    "stat.cc",
  ],
  hdrs = [
    "file.h",
    "fd.h",
    "fs.h",
    "mode.h",
    "options.h",
    "perm.h",
    "registry.h",
    "stat.h",
  ],
  deps = [
    "//base",
    "//base/time",
    "//io",
  ],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "mode_test",
  srcs = ["mode_test.cc"],
  deps = [
    ":core",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "perm_test",
  srcs = ["perm_test.cc"],
  deps = [
    ":core",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "stat_test",
  srcs = ["stat_test.cc"],
  deps = [
    ":core",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_library(
  name = "local",
  srcs = ["local.cc"],
  hdrs = ["local.h"],
  deps = [
    ":core",
    "//path",
  ],
  visibility = ["//visibility:public"],
  alwayslink = True,
)

cc_test(
  name = "local_test",
  srcs = ["local_test.cc"],
  deps = [
    ":core",
    ":local",
    "//base:result_testing",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_library(
  name = "mem",
  srcs = ["mem.cc"],
  hdrs = ["mem.h"],
  deps = [
    ":core",
    "//path",
  ],
  visibility = ["//visibility:public"],
  alwayslink = True,
)

cc_test(
  name = "mem_test",
  srcs = ["mem_test.cc"],
  deps = [
    ":core",
    ":mem",
    "//base:result_testing",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_library(
  name = "file",
  deps = [
    ":core",
    ":local",
    ":mem",
  ],
)

cc_test(
  name = "linker_test",
  srcs = ["linker_test.cc"],
  deps = [
    ":file",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)
