#
#  Copyright (c) 2020 NetEase Inc.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

# https://docs.bazel.build/versions/master/be/c-cpp.html#cc_library

load("//:copts.bzl", "CURVE_DEFAULT_COPTS")

LINKOPTS = select({
    "//:curvebs-sdk": [
        "-Wl,-rpath=/usr/lib/curve",
        "-Wl,-rpath=$$ORIGIN/curve",
        "-Wl,-z,origin",
        "-lbrpc",
        "-lbthread",
        "-lbvar",
        "-lbutil",
        "-ljson2pb",
        "-lmcpack2pb",
        "-lprotoc_lib",
        "-lcc_brpc_internal_proto",
        "-lcc_brpc_idl_options_proto",
        "-lleveldb",
        "-lglog",
        "-lgflags",
        "-lprotobuf",
        "-lprotobuf_lite",
        "-lcurve_common",
        "-lcurve_auth",
        "-lcurve_concurrent",
        "-lnameserver2_proto",
        "-ltopology_proto",
        "-lchunkserver-protos",
        "-lcommon_proto",
    ],
    "//conditions:default": [],
})

cc_library(
    name = "curve_client",
    srcs = glob(
        ["*.cpp"],
    ),
    hdrs = glob(
        ["*h"],
    ),
    copts = CURVE_DEFAULT_COPTS,
    linkopts = LINKOPTS,
    visibility = ["//visibility:public"],
    deps = [
        "//external:brpc",
        "//external:gflags",
        "//external:glog",
        "//external:leveldb",
        "//external:protobuf",
        "//include:include-common",
        "//include/client:include_client",
        "//proto:chunkserver-cc-protos",
        "//proto:nameserver2_cc_proto",
        "//proto:topology_cc_proto",
        "//src/common:curve_auth",
        "//src/common:curve_common",
    ],
)
