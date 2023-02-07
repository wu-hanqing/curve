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

#
# Copyright 2017 The Abseil Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//visibility:public"])

config_setting(
    name = "clang_compiler",
    flag_values = {
        "@bazel_tools//tools/cpp:compiler": "clang",
    },
)

config_setting(
    name = "curvebs-sdk",
    define_values = {
        "curvebs-sdk": "true",
    },
)

filegroup(
    name = "curvebs-exe",
    srcs = [
        "//src/chunkserver",
        "//src/mds/main:curvemds",
        "//src/snapshotcloneserver",
        "//src/tools:curve_chunkserver_tool",
        "//src/tools:curve_format",
        "//src/tools:curve_tool",
        "//tools:curvefsTool",
        "//nebd/src/part2:nebd-server",
        "//nbd/src:curve-nbd",
    ],
)

filegroup(
    name = "curvebs-sdk-prepare",
    srcs = [
        "//src/common:curve_common",
        "//src/common:curve_auth",
        "//src/common/concurrent:curve_concurrent",
        "//proto:nameserver2_cc_proto",
        "//proto:topology_cc_proto",
        "//proto:chunkserver-cc-protos",
        "//proto:common_cc_proto",
        "@com_github_brpc_brpc//:brpc",
        "@com_github_brpc_brpc//:butil",
        "@com_github_brpc_brpc//:bvar",
        "@com_github_brpc_brpc//:bthread",
        "@com_github_brpc_brpc//:json2pb",
        "@com_github_brpc_brpc//:mcpack2pb",
        "@com_github_brpc_brpc//:cc_brpc_idl_options_proto",
        "@com_github_brpc_brpc//:cc_brpc_internal_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:protobuf_lite",
        "@com_google_protobuf//:protoc_lib",
        "@com_github_google_leveldb//:leveldb",
        "@com_github_google_glog//:glog",
        "@com_github_gflags_gflags//:gflags",
    ]
)
