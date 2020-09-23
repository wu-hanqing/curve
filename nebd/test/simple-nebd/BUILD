COPTS = [
    "-DGFLAGS=gflags",
    "-DOS_LINUX",
    "-DSNAPPY",
    "-DHAVE_ZLIB",
    "-DHAVE_SSE42",
    "-DNDEBUG",
    "-std=c++14",
    "-pipe",
    "-W",
    "-Wall",
    "-Wno-unused-parameter",
    "-fPIC",
    "-fno-omit-frame-pointer",
    "-momit-leaf-frame-pointer",
    "-msse4.2",
    "-pthread",
    "-Wsign-compare",
    "-Wno-unused-variable",
    "-Wno-missing-field-initializers",
    "-Woverloaded-virtual",
    "-Wnon-virtual-dtor"
]

cc_binary(
    name = "fake_nebd_server",
    srcs = glob([
        "fake_nebd_server.cpp",
    ]),
    copts = COPTS,
    deps = [
        "//proto:nameserver2_cc_proto",
        "//proto:topology_cc_proto",
        "//proto:chunkserver-cc-protos",
        "//external:glog",
        "//external:brpc",
        "//external:json",
        "//external:gflags",
        "//src/client:curve",
        "//nebd/src/common:nebd_common",
        "//nebd/proto:client_cc_proto"
    ],
)


cc_binary(
    name = "test_rand",
    srcs = glob([
        "test_rand.cpp",
    ]),
    copts = COPTS,
    deps = [
        "//proto:nameserver2_cc_proto",
        "//proto:topology_cc_proto",
        "//proto:chunkserver-cc-protos",
        "//external:glog",
        "//external:brpc",
        "//external:butil",
        "//external:json",
        "//external:gflags",
        "//src/client:curve",
        "//nebd/src/common:nebd_common",
        "//nebd/proto:client_cc_proto"
    ],
)
