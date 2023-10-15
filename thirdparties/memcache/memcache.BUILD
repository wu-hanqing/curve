# Description:
#  Memcache C++ SDK

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "libmemcached",
    srcs = [
        "lib/libmemcached.so",
        "lib/libmemcached.so.11",
    ],
    hdrs = glob([
	"include/**/*.h",
	"include/*.h",
    ]),
    includes = [
        "include/",
    ],
)
