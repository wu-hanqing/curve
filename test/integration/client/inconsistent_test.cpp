#include <glog/logging.h>

#include <atomic>
#include <future>

#include "include/client/libcurve.h"
#include "src/common/concurrent/count_down_event.h"

static const char* confpath = "./client.conf";
static const char* filename = "/vol";

static std::atomic<bool> running{true};
static constexpr size_t sector = 512;
static constexpr size_t block = 4096;
static constexpr size_t length = 10ull * 1024 * 1024 * 1024;

static curve::common::CountDownEvent counter_down;

static char rand_char() {
    static char b[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static size_t len = strlen(b);
    return b[rand() % len];
}

static void callback(CurveAioContext* ctx) {
    CHECK(ctx->ret == 0 || ctx->ret == ctx->length);
    counter_down.Signal();
    delete ctx;
}

static void issue_aio_request(int fd, size_t offset, size_t size, char* data) {
    CurveAioContext* ctx = new CurveAioContext{};
    ctx->op = LIBCURVE_OP::LIBCURVE_OP_WRITE;
    ctx->offset = offset;
    ctx->length = size;
    ctx->buf = data;
    ctx->cb = callback;

    AioWrite(fd, ctx);
}

static void write_and_check(int fd) {
    // random select one 4k block
    size_t offset = rand() % (length / block - 1) * block;
    char origin_data[block];
    for (auto& c : origin_data) {
        c = rand_char();
    }
    auto ret = Write(fd, origin_data, offset, block);
    CHECK(ret == 0 || ret == block);

    ret = Read(fd, origin_data, offset, block);
    CHECK(ret == 0 || ret == block);

    // issue two 512 bytes requests within same 4k block
    counter_down.Reset(2);

    char data1[sector];
    memset(data1, rand_char(), sizeof(data1));
    size_t offset1 = offset;
    size_t size1 = sector;

    char data2[sector];
    memset(data2, rand_char(), sizeof(data2));
    size_t offset2 = offset + block - sector;
    size_t size2 = sector;

    auto f1 = std::async(std::launch::async, issue_aio_request, fd, offset1,
                         size1, data1);
    auto f2 = std::async(std::launch::async, issue_aio_request, fd, offset2,
                         size2, data2);

    // wait both requests finish
    counter_down.Wait();

    // check data
    char expected_data[block];
    memcpy(expected_data, origin_data, sizeof(origin_data));
    memcpy(expected_data, data1, sizeof(data1));
    memcpy(expected_data + block - sector, data2, sizeof(data2));

    char real_data[block];
    ret = Read(fd, real_data, offset, block);
    CHECK(ret == 0 || ret == block);

    LOG(INFO) << "expected data: " << std::string{expected_data, block};
    LOG(INFO) << "real data: " << std::string{real_data, block};
    LOG(INFO) << "origin data: " << std::string{origin_data, block};
    LOG(INFO) << "first sector write data: " << std::string{data1, sector};
    LOG(INFO) << "last sector write data: " << std::string{data2, sector};

    CHECK(0 == memcmp(expected_data, real_data, sizeof(real_data)));
}

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    int ret = Init(confpath);
    if (ret != 0) {
        LOG(ERROR) << "Init failed";
        return 0;
    }

    C_UserInfo userInfo;
    memset(&userInfo, 0, sizeof(userInfo));
    strcpy(userInfo.owner, "test");

    int fd = Open(filename, &userInfo);
    if (fd < 0) {
        LOG(ERROR) << "open failed";
        return 0;
    }

    size_t count = 0;
    while (running.load(std::memory_order_relaxed)) {
        LOG(INFO) << "iteration " << ++count;
        write_and_check(fd);
    }

    google::ShutdownGoogleLogging();
    return 0;
}
