/*
 *  Copyright (c) 2023 NetEase Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef SRC_CHUNKSERVER_RAFTLOG_COROUTINE_IO_PROXY_H_
#define SRC_CHUNKSERVER_RAFTLOG_COROUTINE_IO_PROXY_H_

#include <butil/atomicops.h>
#include <butil/memory/ref_counted.h>
#include <bvar/passive_status.h>
#include <sys/uio.h>

#include <atomic>
#include <limits>
#include <thread>
#include <vector>

#include "blockingconcurrentqueue.h"  // NOLINT
#include "include/curve_compiler_specific.h"

namespace curve {
namespace chunkserver {

class IOProxy;

class IOTask : public butil::RefCountedThreadSafe<IOTask> {
 public:
    enum Op { SYNC, READ, WRITE };

    // Create (p)readv/writev task
    static scoped_refptr<IOTask> Create(int fd,
                                        Op op,
                                        off_t offset,
                                        int iovcnt,
                                        struct iovec* iov);

    // Create sync task
    static scoped_refptr<IOTask> Create(int fd, Op op);

    // Wail until task finished, and return result
    ssize_t Wait();

    Op op;
    int fd;
    off_t offset;
    int iovcnt;
    struct iovec* iov;
    ssize_t res;
    int error;
    butil::atomic<int>* butex;

 private:
    friend class butil::RefCountedThreadSafe<IOTask>;
    friend class IOProxy;

    static constexpr int kStopTaskFd = std::numeric_limits<int>::min();

    void Reset();

    IOTask() = default;
    ~IOTask();

    IOTask(const IOTask&) = delete;
    IOTask& operator=(const IOTask&) = delete;
};

class IOProxy {
 public:
    static IOProxy& Instance();

    void Enqueue(IOTask* task);

 private:
    IOProxy() : running_{false} {}

    ~IOProxy();

    IOProxy(const IOProxy&) = delete;
    IOProxy& operator=(const IOProxy&) = delete;

    bool Init();

    void Run();

    static void ExecuteTask(IOTask* task,
                            bool wake = false,
                            bool decrease = false);

    alignas(64) moodycamel::BlockingConcurrentQueue<IOTask*> task_q_;
    alignas(64) std::vector<std::thread> workers_;
    alignas(64) std::atomic<bool> running_;
    alignas(64) static std::atomic<int32_t> inflight_bthread_io;
    alignas(64) static bvar::PassiveStatus<int32_t> inflight_bthread_io_bvar;
};

}  // namespace chunkserver
}  // namespace curve

#endif  // SRC_CHUNKSERVER_RAFTLOG_COROUTINE_IO_PROXY_H_
