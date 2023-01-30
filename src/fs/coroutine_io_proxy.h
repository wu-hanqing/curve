#pragma once

#include <butil/atomicops.h>
#include <butil/memory/ref_counted.h>
#include <bvar/passive_status.h>
#include <sys/uio.h>

#include <atomic>
#include <thread>
#include <vector>

#include "blockingconcurrentqueue.h"

namespace curve {
namespace fs {

class IOTask : public butil::RefCountedThreadSafe<IOTask> {
 public:
    enum Op { READ, WRITE, SYNC };

    static scoped_refptr<IOTask> Create(int fd,
                                        Op op,
                                        off_t offset,
                                        int niov,
                                        struct iovec* iov,
                                        bool dma);

    static scoped_refptr<IOTask> Create(int fd, Op op);

    ssize_t Wait();

 private:
    friend class butil::RefCountedThreadSafe<IOTask>;
    friend class IODelegate;

    static constexpr int kStopTaskFd = std::numeric_limits<int>::min();

    IOTask() = default;
    ~IOTask();

    IOTask(const IOTask&) = delete;
    IOTask& operator=(const IOTask&) = delete;
    IOTask(IOTask&&) noexcept = delete;
    IOTask& operator=(IOTask&&) noexcept = delete;

    int fd;
    Op op;
    off_t offset;
    int niov;
    bool dma;
    int error = 0;
    struct iovec* iov;
    ssize_t res;
    butil::atomic<int>* butex;
};

class IOProxy {
 public:
    static IOProxy& Instance();

    void enqueue(IOTask* task);

 private:
    IOProxy() : running_{false} {}

    ~IOProxy();

    IOProxy(const IOProxy&) = delete;
    IOProxy& operator=(const IOProxy&) = delete;

    bool init();

    void run();

    static void execute_task(IOTask* task,
                             bool wake = false,
                             bool decrease = false);

    alignas(64) moodycamel::BlockingConcurrentQueue<IOTask*> task_q_;
    alignas(64) std::vector<std::thread> workers_;
    alignas(64) std::atomic<bool> running_;
};

}  // namespace fs
}  // namespace curve
