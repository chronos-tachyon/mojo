# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "rand",
  srcs = [
    "lcg.cc",
    "lfsr.cc",
    "mt.cc",
    "rand.cc",
    "xorshift.cc",
  ],
  hdrs = [
    "rand.h",
  ],
  deps = [
    "//base",
  ],
  visibility = ["//visibility:public"],
)

cc_binary(
  name = "demo",
  srcs = ["demo.cc"],
  deps = [":rand"],
)
