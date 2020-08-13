#include <brpc/channel.h>
#include <brpc/controller.h>
#include <bthread/execution_queue.h>
#include <butil/time.h>
#include <bvar/bvar.h>
#include <gflags/gflags.h>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

#include "nebd/proto/client.pb.h"
#include "src/common/concurrent/count_down_event.h"
#include "src/common/timeutility.h"

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

brpc::Channel channel;
std::atomic<uint32_t> finishCount(1);
curve::common::ExpiredTime t;

bvar::LatencyRecorder rpcLatency("rpc_latency");

constexpr uint32_t BufSize = 4 * 1024;
char buffer[BufSize];

void TrivialDeleter(void* ptr) {}

const char* UnixSock = "/tmp/unix.pingpong.sock";
const char* TcpAddress = "localhost:12345";

DEFINE_int32(times, 100 * 100 * 100, "test times");
DEFINE_bool(attachment, true, "rpc with 4K attachment");
DEFINE_bool(tcp, true, "use tcp or unix");
DEFINE_string(tcpAddress, TcpAddress, "tcp address");
DEFINE_int32(parallel, 1, "parallel send");

std::vector<double> lats;
std::mutex mtx;

struct TestRequestClosure : public google::protobuf::Closure {
    void Run() override {}

    brpc::Controller cntl;
    nebd::client::WriteRequest request;
    nebd::client::WriteResponse response;
};

void SendWriteRequest() {
    TestRequestClosure* done = new TestRequestClosure();
    done->request.set_fd(INT_MAX);
    done->request.set_offset(ULLONG_MAX);
    done->request.set_size(ULLONG_MAX);

    nebd::client::NebdFileService_Stub stub(&channel);
    done->cntl.set_timeout_ms(-1);

    // append 4K data
    if (FLAGS_attachment) {
        done->cntl.request_attachment().append_user_data(
            buffer, BufSize, TrivialDeleter);
    }

    stub.Write(&done->cntl, &done->request, &done->response, nullptr);
    if (done->cntl.Failed()) {
        std::cout << "rpc failed, " << done->cntl.ErrorText() << std::endl;
        _exit(1);
    }

    auto latencyUs = done->cntl.latency_us();
    lats.push_back(latencyUs);

    delete done;
}

void StartTest() {
    auto func = [](int times) {
        for (int i = 0; i < times; ++i) {
            SendWriteRequest();
        };
    };

    lats.reserve(FLAGS_times);

    std::vector<std::thread> backThreads;
    for (int i = 0; i < FLAGS_parallel; ++i) {
        backThreads.push_back(std::thread(func, FLAGS_times / FLAGS_parallel));
    }

    for (auto& th : backThreads) {
        th.join();
    }

    double totalUs = std::accumulate(lats.begin(), lats.end(), 0.0);
    std::sort(lats.begin(), lats.end());
    std::cout << "avg latency us: " << totalUs / lats.size()
              << ", min: " << lats.front() << ", max: " << lats.back() << "\n";
}

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);

    ::memset(buffer, 1, sizeof(buffer));

    int ret = 0;
    if (FLAGS_tcp) {
        ret = channel.Init(FLAGS_tcpAddress.c_str(), nullptr);
    } else {
        ret = channel.InitWithSockFile(UnixSock, nullptr);
    }

    if (ret != 0) {
        std::cout << "channel Init failed";
        return -1;
    }

    std::cout << "times: " << FLAGS_times
              << ", with attachment: " << FLAGS_attachment
              << ", tcp: " << FLAGS_tcp << ", parallel: " << FLAGS_parallel
              << std::endl;

    StartTest();

    // while (true) {
    //     sleep(1);
    // }

    return 0;
}
