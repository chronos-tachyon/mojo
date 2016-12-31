# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "net",
  deps = [
    ":core",
    ":inet",
    ":unix",
  ],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "core",
  srcs = [
    "addr.cc",
    "conn.cc",
    "connfd.cc",
    "internal.cc",
    "internal.h",
    "net.cc",
    "options.cc",
    "protocol.cc",
    "registry.cc",
    "sockopt.cc",
  ],
  hdrs = [
    "addr.h",
    "conn.h",
    "connfd.h",
    "net.h",
    "options.h",
    "protocol.h",
    "registry.h",
    "sockopt.h",
  ],
  deps = [
    "//base",
    "//event",
    "//io",
  ],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "fake",
  srcs = ["fake.cc"],
  hdrs = ["fake.h"],
  deps = [":core"],
  visibility = ["//visibility:public"],
  testonly = True,
)

cc_test(
  name = "fake_test",
  srcs = ["fake_test.cc"],
  deps = [
    ":core",
    ":fake",
    "//base:result_testing",
    "//external:gtest",
  ],
  timeout = "short",
  size = "small",
)

cc_test(
  name = "registry_test",
  srcs = ["registry_test.cc"],
  deps = [
    ":core",
    ":fake",
    "//base:result_testing",
    "//external:gtest",
  ],
  timeout = "short",
  size = "small",
)

cc_test(
  name = "sockopt_test",
  srcs = ["sockopt_test.cc"],
  deps = [
    ":core",
    "//base:result_testing",
    "//external:gtest",
  ],
  timeout = "short",
  size = "small",
)

cc_library(
  name = "testing",
  srcs = ["testing.cc"],
  hdrs = ["testing.h"],
  deps = [
    ":core",
    "//base:result_testing",
    "//external:gtest_nomain",
  ],
  visibility = ["//visibility:public"],
  testonly = True,
)

cc_library(
  name = "inet",
  srcs = [
    "inet.cc",
    "ip.cc",
  ],
  hdrs = [
    "inet.h",
    "ip.h",
  ],
  deps = [
    ":core",
    "//base",
  ],
  linkopts = ["-lanl"],
  visibility = ["//visibility:public"],
  alwayslink = True,
)

cc_test(
  name = "ip_test",
  srcs = ["ip_test.cc"],
  deps = [
    ":inet",
    "//base:result_testing",
    "//external:gtest",
  ],
  timeout = "short",
  size = "small",
)

cc_test(
  name = "inet_test",
  srcs = ["inet_test.cc"],
  deps = [
    ":inet",
    ":testing",
    "//base:result_testing",
    "//external:gtest",
  ],
  timeout = "short",
  size = "small",
)

cc_library(
  name = "unix",
  srcs = ["unix.cc"],
  hdrs = ["unix.h"],
  deps = [
    ":core",
    "//base",
  ],
  visibility = ["//visibility:public"],
  alwayslink = True,
)

cc_test(
  name = "unix_test",
  srcs = ["unix_test.cc"],
  deps = [
    ":unix",
    ":testing",
    "//base:result_testing",
    "//external:gtest",
  ],
  timeout = "short",
  size = "small",
)

cc_binary(
  name = "resolve_tester",
  srcs = ["resolve_tester.cc"],
  deps = [":net"],
)
