
COPTS = [
    "-DGFLAGS=gflags",
    "-DOS_LINUX",
    "-DSNAPPY",
    "-DHAVE_ZLIB",
    "-DHAVE_SSE42",
    "-DNDEBUG",
    "-std=c++11",
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
    name = "pingpong-client-async",
    srcs = glob([
        "client-async.cpp",
        ]
    ),
    deps = [
        "//external:brpc",
        "//nebd/proto:client_cc_proto",
        "//src/common/concurrent:curve_concurrent",
        "//src/common:curve_common"

    ],
    copts = COPTS,
)


cc_binary(
    name = "pingpong-client-async-like",
    srcs = glob([
        "client-async-like.cpp",
        ]
    ),
    deps = [
        "//external:brpc",
        "//nebd/proto:client_cc_proto",
        "//src/common/concurrent:curve_concurrent",
        "//src/common:curve_common"

    ],
    copts = COPTS,
)

cc_binary(
    name = "pingpong-client-sync",
    srcs = glob([
        "client-sync.cpp",
        ]
    ),
    deps = [
        "//external:brpc",
        "//nebd/proto:client_cc_proto",
        "//src/common/concurrent:curve_concurrent",
        "//src/common:curve_common"

    ],
    copts = COPTS,
)

cc_binary(
    name = "pingpong-server",
    srcs = glob([
        "server.cpp",
        ]
    ),
    deps = [
        "//external:brpc",
        "//nebd/proto:client_cc_proto",
        "//src/common/concurrent:curve_concurrent",
        "//src/common:curve_common"
    ],
    copts = COPTS,
)
