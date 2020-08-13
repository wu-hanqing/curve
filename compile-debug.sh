bazel build //nebd/test/pingpong-test/... --copt -DHAVE_ZLIB=1 --compilation_mode=dbg -s --define=with_glog=true \
    --define=libunwind=true --copt -DGFLAGS_NS=google --copt \
    -Wno-error=format-security --copt -DUSE_BTHREAD_MUTEX