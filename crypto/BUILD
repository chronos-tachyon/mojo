# https://bazel.build/
# vim: set ft=python sts=2 sw=2 et:

cc_library(
    name = "crypto",
    visibility = ["//visibility:public"],
    deps = [
        ":core",
        "//crypto/cipher:aes",
        "//crypto/cipher:cbc",
        "//crypto/cipher:cfb",
        "//crypto/cipher:chacha20",
        "//crypto/cipher:ctr",
        "//crypto/cipher:des",
        "//crypto/cipher:ecb",
        "//crypto/cipher:ofb",
        "//crypto/cipher:rc4",
        "//crypto/cipher:salsa20",
        "//crypto/hash:md5",
        "//crypto/hash:sha1",
        "//crypto/hash:sha2",
        "//crypto/hash:sha3",
    ],
)

cc_library(
    name = "core",
    srcs = [
        "crypto.cc",
        "primitives.cc",
        "security.cc",
        "subtle.cc",
    ],
    hdrs = [
        "crypto.h",
        "primitives.h",
        "security.h",
        "subtle.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//io",
    ],
)

cc_binary(
    name = "cryptotool",
    srcs = ["cryptotool.cc"],
    deps = [
        ":crypto",
        "//encoding:base64",
        "//encoding:hex",
    ],
)
