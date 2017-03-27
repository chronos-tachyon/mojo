# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "base64",
  srcs = ["base64.cc"],
  hdrs = ["base64.h"],
  deps = ["//base"],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "hex",
  srcs = ["hex.cc"],
  hdrs = ["hex.h"],
  deps = ["//base"],
  visibility = ["//visibility:public"],
)

cc_test(
  name = "base64_test",
  srcs = ["base64_test.cc"],
  deps = [
    ":base64",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)

cc_test(
  name = "hex_test",
  srcs = ["hex_test.cc"],
  deps = [
    ":hex",
    "//external:gtest",
  ],
  size = "small",
  timeout = "short",
)
