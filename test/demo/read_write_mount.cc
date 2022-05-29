#include <gflags/gflags.h>
#include <thread>

#include "include/client/libcurve.h"
#include "include/client/libcurve_define.h"
#include <glog/logging.h>

#include <chrono>
#include <mutex>
#include <condition_variable>
#include <iostream>

using namespace std::chrono_literals;
using namespace curve::client;

DEFINE_bool(create, false, "create file");

DEFINE_string(filename, "/rw", "filename");

DEFINE_string(conf, "", "client configuration file");

DEFINE_bool(open_rw, false, "open with read/write, other open read only");

DEFINE_bool(issue_write, false, "issue write request");

DEFINE_bool(close, false, "close file at exit");

std::mutex mtx;
std::condition_variable cond;
bool finished = false;

void callback(CurveAioContext* context) {
    std::lock_guard<std::mutex> lock(mtx);
    finished = true;
    cond.notify_one();
}

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);

    CurveClient client;
    CHECK(client.Init(FLAGS_conf) == 0);

    UserInfo user;
    user.owner = "test";

    if (FLAGS_create) {
        const int ret = client.Create(FLAGS_filename, user, 10ULL << 30);
        CHECK(ret == 0 || ret == -LIBCURVE_ERROR::EXISTS);
    }

    int flags = CURVE_SHARED;
    if (FLAGS_open_rw) {
        flags |= CURVE_RDWR;
    } else {
        flags |= CURVE_RDONLY;
    }

    int fd = client.Open(FLAGS_filename + "_test_", flags);
    CHECK(fd >= 0);

    LOG(INFO) << "Open succeeded";

    char data[4096];
    if (FLAGS_issue_write) {
        CurveAioContext context;
        memset(&context, 0, sizeof(context));
        context.offset = 0;
        context.length = 4 * 1024;
        context.op = LIBCURVE_OP::LIBCURVE_OP_WRITE;
        context.cb = callback;
        context.buf = data;
        
        int ret = client.AioWrite(fd, &context, UserDataType::RawBuffer);
        if (FLAGS_open_rw) {
            CHECK(ret >= 0);
        } else {
            CHECK(ret == -1);
            LOG(INFO) << "getchar()";
            getchar();
        }

        {
            std::unique_lock<std::mutex> lock(mtx);
            cond.wait(lock, []() { return finished; });
        }

        if (FLAGS_open_rw) {
            CHECK(context.ret == 0 || context.ret == context.length)
                << context.ret;
        } else {
            CHECK(context.ret < 0) << context.ret;
        }
    }

    getchar();

    if (FLAGS_close) {
        CHECK(0 == client.Close(fd));
        std::cerr << "closed\n";
    }

    return 0;
}
