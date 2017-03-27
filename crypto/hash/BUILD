# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
  name = "hash",
  srcs = [
    "dummy.cc",
    "hash.cc",
    "keccak.cc",
    "md5.cc",
    "sha1.cc",
    "sha2-256.cc",
    "sha2-512.cc",
    "sha3.cc",
  ],
  hdrs = [
    "hash.h",
    "keccak.h",
  ],
  deps = [
    "//base",
    "//crypto/common",
    "//encoding:base64",
    "//encoding:hex",
    "//io",
  ],
  visibility = ["//visibility:public"],
)

cc_binary(
  name = "hashutil",
  srcs = ["hashutil.cc"],
  deps = [":hash"],
)
