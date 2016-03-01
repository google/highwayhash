# SipHash and HighwayHash: cryptographically-strong pseudorandom functions

package(
    default_hdrs_check = "strict",
#---------------
    # TBB does not export headers.
    features = ["-layering_check"],
)

cc_library(
    name = "vector",
    hdrs = [
        "code_annotation.h",
        "vec.h",
        "vec2.h",
    ],
)

cc_library(
    name = "sip_hash",
    srcs = ["sip_hash.cc"],
    hdrs = ["sip_hash.h"],
    deps = [
        ":vector",
    ],
)

cc_library(
    name = "sip_tree_hash",
    srcs = ["sip_tree_hash.cc"],
    hdrs = ["sip_tree_hash.h"],
    deps = [
        ":sip_hash",
        ":vector",
    ],
)

cc_library(
    name = "scalar_sip_tree_hash",
    srcs = ["scalar_sip_tree_hash.cc"],
    hdrs = ["scalar_sip_tree_hash.h"],
    deps = [
        ":sip_hash",
        ":vector",
    ],
)

cc_library(
    name = "highway_tree_hash",
    srcs = ["highway_tree_hash.cc"],
    hdrs = ["highway_tree_hash.h"],
    deps = [
        ":vector",
    ],
)

cc_library(
    name = "scalar_highway_tree_hash",
    srcs = ["scalar_highway_tree_hash.cc"],
    hdrs = ["scalar_highway_tree_hash.h"],
    deps = [
        ":vector",
    ],
)

cc_binary(
    name = "sip_hash_main",
    srcs = [
        "sip_hash_main.cc",
    ],
    deps = [
        ":highway_tree_hash",
        ":scalar_highway_tree_hash",
        ":scalar_sip_tree_hash",
        ":sip_hash",
        ":sip_tree_hash",
        ":smhasher",
    ],
)

#-----------------------

cc_library(
    name = "smhasher",
    srcs = ["smhasher.cc"],
    hdrs = ["smhasher.h"],
    deps = [
        ":highway_tree_hash",
        ":scalar_highway_tree_hash",
        ":scalar_sip_tree_hash",
        ":sip_hash",
        ":sip_tree_hash",
        "//base",
        "//third_party/smhasher:libsmhasher",
        "//third_party/tbb",
        "//thread",
        "//util/hash/hasheval:hasheval_lib",
    ],
)

cc_binary(
    name = "pictures",
    srcs = [
        "pictures.cc",
    ],
    deps = [
        ":highway_tree_hash",
    ],
)

cc_binary(
    name = "histopics",
    srcs = [
        "histopics.cc",
    ],
    deps = [
        "//base",
        "//thread",
        ":highway_tree_hash",
    ],
)
