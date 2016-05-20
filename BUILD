# SipHash and HighwayHash: cryptographically-strong pseudorandom functions

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "vector",
    hdrs = [
        "highwayhash/code_annotation.h",
        "highwayhash/types.h",
        "highwayhash/vec.h",
        "highwayhash/vec2.h",
    ],
)

cc_library(
    name = "sip_hash",
    srcs = ["highwayhash/sip_hash.cc"],
    hdrs = [
        "highwayhash/code_annotation.h",
        "highwayhash/sip_hash.h",
        "highwayhash/state_helpers.h",
        "highwayhash/types.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "sip_tree_hash",
    srcs = ["highwayhash/sip_tree_hash.cc"],
    hdrs = ["highwayhash/sip_tree_hash.h"],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        ":sip_hash",
        ":vector",
    ],
)

cc_library(
    name = "scalar_sip_tree_hash",
    srcs = ["highwayhash/scalar_sip_tree_hash.cc"],
    hdrs = ["highwayhash/scalar_sip_tree_hash.h"],
    includes = ["."],
    deps = [
        ":sip_hash",
        ":vector",
    ],
)

cc_library(
    name = "highway_tree_hash",
    srcs = ["highwayhash/highway_tree_hash.cc"],
    hdrs = [
        "highwayhash/highway_tree_hash.h",
        "highwayhash/state_helpers.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        ":vector",
    ],
)

cc_library(
    name = "sse41_highway_tree_hash",
    srcs = ["highwayhash/sse41_highway_tree_hash.cc"],
    hdrs = ["highwayhash/sse41_highway_tree_hash.h"],
    includes = ["."],
    deps = [
        ":vector",
    ],
)

cc_library(
    name = "scalar_highway_tree_hash",
    srcs = ["highwayhash/scalar_highway_tree_hash.cc"],
    hdrs = ["highwayhash/scalar_highway_tree_hash.h"],
    includes = ["."],
    deps = [
        ":vector",
    ],
)

cc_binary(
    name = "sip_hash_main",
    srcs = [
        "highwayhash/sip_hash_main.cc",
    ],
    includes = ["."],
    deps = [
        ":highway_tree_hash",
        ":scalar_highway_tree_hash",
        ":scalar_sip_tree_hash",
        ":sip_hash",
        ":sip_tree_hash",
        ":sse41_highway_tree_hash",
        ":vector",
    ],
)
