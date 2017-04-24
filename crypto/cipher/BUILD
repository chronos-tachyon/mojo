# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
    name = "internal",
    hdrs = [
        "cbc_internal.h",
        "ctr_internal.h",
        "gcm_internal.h",
    ],
    deps = [
        "//base",
        "//crypto:core",
    ],
)

cc_library(
    name = "cbc",
    srcs = ["cbc.cc"],
    hdrs = ["cbc.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":internal",
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_library(
    name = "cfb",
    srcs = ["cfb.cc"],
    hdrs = ["cfb.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_library(
    name = "ctr",
    srcs = ["ctr.cc"],
    hdrs = ["ctr.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":internal",
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_library(
    name = "ecb",
    srcs = ["ecb.cc"],
    hdrs = ["ecb.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_library(
    name = "gcm",
    srcs = ["gcm.cc"],
    hdrs = ["gcm.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":internal",
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_library(
    name = "ofb",
    srcs = ["ofb.cc"],
    hdrs = ["ofb.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_library(
    name = "des",
    srcs = [
        "_des.h",
        "des.cc",
    ],
    hdrs = [
        "des.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_test(
    name = "des_test",
    size = "small",
    timeout = "short",
    srcs = ["des_test.cc"],
    deps = [
        ":des",
        "//encoding:hex",
        "//external:gtest",
    ],
)

cc_library(
    name = "aes",
    srcs = [
        "aes_internal.h",
        "aes.cc",
        "aes_generic.cc",
        "aes_amd64.cc",
    ],
    hdrs = [
        "aes.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":internal",
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
    copts = ["-mssse3", "-maes"],
)

cc_test(
    name = "aes_test",
    size = "small",
    timeout = "short",
    srcs = ["aes_test.cc"],
    deps = [
        ":aes",
        ":cbc",
        ":ctr",
        ":gcm",
        "//base:result_testing",
        "//encoding:hex",
        "//external:gtest",
    ],
)

cc_library(
    name = "rc4",
    srcs = ["rc4.cc"],
    hdrs = ["rc4.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_test(
    name = "rc4_test",
    size = "small",
    timeout = "short",
    srcs = ["rc4_test.cc"],
    deps = [
        ":rc4",
        "//encoding:hex",
        "//external:gtest",
    ],
)

cc_library(
    name = "salsa20",
    srcs = ["salsa20.cc"],
    hdrs = ["salsa20.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_library(
    name = "chacha20",
    srcs = ["chacha20.cc"],
    hdrs = ["chacha20.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//crypto:core",
    ],
    alwayslink = True,
)

cc_test(
    name = "chacha20_test",
    size = "small",
    timeout = "short",
    srcs = ["chacha20_test.cc"],
    deps = [
        ":chacha20",
        "//base:result_testing",
        "//encoding:hex",
        "//external:gtest",
    ],
)
