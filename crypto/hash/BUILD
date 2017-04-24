# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
    name = "md5",
    srcs = ["md5.cc"],
    hdrs = ["md5.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_library(
    name = "sha1",
    srcs = ["sha1.cc"],
    hdrs = ["sha1.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_library(
    name = "sha2",
    srcs = [
        "sha2-256.cc",
        "sha2-512.cc",
    ],
    hdrs = ["sha2.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_library(
    name = "sha3",
    srcs = [
        "keccak.cc",
        "sha3.cc",
    ],
    hdrs = [
        "keccak.h",
        "sha3.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)
