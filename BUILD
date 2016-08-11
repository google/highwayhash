# SipHash and HighwayHash: cryptographically-strong pseudorandom functions

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "code_annotation",
    hdrs = [
        "highwayhash/code_annotation.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "vector",
    hdrs = [
        "highwayhash/types.h",
        "highwayhash/vec.h",
        "highwayhash/vec2.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        ":code_annotation",
    ],
)

cc_library(
    name = "os_specific",
    srcs = ["highwayhash/os_specific.cc"],
    hdrs = [
        "highwayhash/os_specific.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        ":code_annotation",
    ],
)

cc_library(
    name = "tsc_timer",
    hdrs = [
        "highwayhash/tsc_timer.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        ":code_annotation",
        ":os_specific",
    ],
)

cc_library(
    name = "profiler",
    hdrs = [
        "highwayhash/profiler.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        ":code_annotation",
        ":tsc_timer",
    ],
)

cc_binary(
    name = "profiler_example",
    srcs = ["highwayhash/profiler_example.cc"],
    deps = [
        ":os_specific",
        ":profiler",
    ],
)

cc_library(
    name = "nanobenchmark",
    hdrs = [
        "highwayhash/nanobenchmark.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        ":code_annotation",
        ":os_specific",
        ":tsc_timer",
    ],
)

cc_binary(
    name = "nanobenchmark_example",
    srcs = [
        "highwayhash/nanobenchmark_example.cc",
    ],
    includes = ["."],
    deps = [
        ":nanobenchmark",
        ":os_specific",
    ],
)

cc_library(
    name = "data_parallel",
    hdrs = [
        "highwayhash/data_parallel.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "data_parallel_test",
    size = "small",
    srcs = ["highwayhash/data_parallel_test.cc"],
    deps = [
        ":data_parallel",
        "//base",
        "//testing/base/public:gunit_main_no_google3",
        "//thread",
    ],
)

cc_library(
    name = "sip_hash",
    srcs = ["highwayhash/sip_hash.cc"],
    hdrs = [
        "highwayhash/sip_hash.h",
        "highwayhash/state_helpers.h",
        "highwayhash/types.h",
    ],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        ":code_annotation",
    ],
)

cc_library(
    name = "sip_tree_hash",
    srcs = ["highwayhash/sip_tree_hash.cc"],
    hdrs = ["highwayhash/sip_tree_hash.h"],
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = [
        ":code_annotation",
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
        ":code_annotation",
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
        ":code_annotation",
        ":vector",
    ],
)

cc_library(
    name = "sse41_highway_tree_hash",
    srcs = ["highwayhash/sse41_highway_tree_hash.cc"],
    hdrs = [
        "highwayhash/sse41_highway_tree_hash.h",
        "highwayhash/state_helpers.h",
    ],
    includes = ["."],
    deps = [
        ":code_annotation",
        ":vector",
    ],
)

cc_library(
    name = "scalar_highway_tree_hash",
    srcs = ["highwayhash/scalar_highway_tree_hash.cc"],
    hdrs = [
        "highwayhash/scalar_highway_tree_hash.h",
        "highwayhash/state_helpers.h",
    ],
    includes = ["."],
    deps = [
        ":code_annotation",
        ":vector",
    ],
)

cc_test(
    name = "sip_hash_test",
    size = "small",
    srcs = ["highwayhash/sip_hash_test.cc"],
    deps = [
        ":highway_tree_hash",
        ":scalar_highway_tree_hash",
        ":scalar_sip_tree_hash",
        ":sip_hash",
        ":sip_tree_hash",
        ":sse41_highway_tree_hash",
        "//base",
        "//testing/base/public:gunit_main",
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
        ":nanobenchmark",
        ":os_specific",
        ":scalar_highway_tree_hash",
        ":scalar_sip_tree_hash",
        ":sip_hash",
        ":sip_tree_hash",
        ":sse41_highway_tree_hash",
        ":vector",
    ],
)
