package(default_visibility = ["//visibility:public"])

cc_library(
    name = "concurrentqueue",
    hdrs = [
        "blockingconcurrentqueue.h",
        "concurrentqueue.h",
        "lightweightsemaphore.h",
    ],
    copts = [
        "-std=c++11",
    ],
    linkopts = [
        "-pthread",
    ],
)
