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

#include "src/chunkserver/raftlog/coroutine_io_proxy.h"

#include <braft/fsync.h>
#include <bthread/bthread.h>
#include <bthread/butex.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <sys/uio.h>

#include <string>

namespace curve {
namespace chunkserver {

namespace {

bool ValidBthreadIOThreshold(const char* /*filename*/, int32_t /*value*/) {
    return true;
}

int32_t GetInflightBthreadIOCount(void* arg) {
    return static_cast<std::atomic<int32_t>*>(arg)->load(
            std::memory_order_relaxed);
}

void SetThreadName() {
    static std::atomic<int> count{1};
    int id = count.fetch_add(1, std::memory_order_relaxed);
    std::string name = "io-" + std::to_string(id);
    pthread_setname_np(pthread_self(), name.c_str());
}

}  // namespace

DEFINE_int32(io_worker_num, 10, "xxx");
DEFINE_int32(bthread_io_threshold,
             40,
             "Maximum number of IO requests executed in bthread");
DEFINE_validator(bthread_io_threshold, ValidBthreadIOThreshold);

std::atomic<int32_t> IOProxy::inflight_bthread_io{0};

bvar::PassiveStatus<int32_t> IOProxy::inflight_bthread_io_bvar{
        "inflight_bthread_io_count", GetInflightBthreadIOCount,
        &IOProxy::inflight_bthread_io};

scoped_refptr<IOTask> IOTask::Create(int fd, Op op) {
    assert(op == IOTask::SYNC);

    scoped_refptr<IOTask> task(new IOTask{});
    task->fd = fd;
    task->op = op;

    task->Reset();

    auto& proxy = IOProxy::Instance();
    proxy.Enqueue(task);

    return task;
}

scoped_refptr<IOTask> IOTask::Create(int fd,
                                     Op op,
                                     off_t offset,
                                     int iovcnt,
                                     struct iovec* iov) {
    scoped_refptr<IOTask> task(new IOTask{});
    task->fd = fd;
    task->op = op;
    task->offset = offset;
    task->iovcnt = iovcnt;
    task->iov = iov;

    task->Reset();

    auto& proxy = IOProxy::Instance();
    proxy.Enqueue(task);

    return task;
}

void IOTask::Reset() {
    res = -1;
    error = 0;
    butex = bthread::butex_create_checked<butil::atomic<int>>();
    CHECK(butex);
    butex->store(0, std::memory_order_relaxed);
}

IOTask::~IOTask() {
    bthread::butex_destroy(butex);
}

ssize_t IOTask::Wait() {
    while (butex->load(std::memory_order_acquire) != 1) {
        bthread::butex_wait(butex, 0, nullptr);
    }

    return res;
}

IOProxy& IOProxy::Instance() {
    static IOProxy instance;
    static std::once_flag init_once;
    std::call_once(init_once, []() { LOG_IF(FATAL, !instance.Init()); });

    return instance;
}

IOProxy::~IOProxy() {
    if (!running_) {
        return;
    }

    running_.store(false);

    std::vector<IOTask*> stop_tasks;
    for (size_t i = 0; i < workers_.size(); ++i) {
        stop_tasks.emplace_back(new IOTask());
        stop_tasks.back()->AddRef();
        stop_tasks.back()->fd = IOTask::kStopTaskFd;
        task_q_.enqueue(stop_tasks.back());
    }

    for (auto& worker : workers_) {
        worker.join();
    }

    for (auto* task : stop_tasks) {
        task->Release();
    }
}

bool IOProxy::Init() {
    for (int i = 0; i < FLAGS_io_worker_num; ++i) {
        workers_.emplace_back(&IOProxy::Run, this);
    }

    running_.store(true);
    return true;
}

void IOProxy::Enqueue(IOTask* task) {
    task->AddRef();  // release in `ExecuteTask`

    if (bthread_self() != INVALID_BTHREAD &&
        inflight_bthread_io.fetch_add(1, std::memory_order_relaxed) >
                FLAGS_bthread_io_threshold) {
        inflight_bthread_io.fetch_sub(1, std::memory_order_relaxed);
        thread_local moodycamel::ProducerToken token{task_q_};

        if (task_q_.enqueue(token, task)) {
            return;
        }
    }

    ExecuteTask(task, false, true);
}

void IOProxy::ExecuteTask(IOTask* task, bool wake, bool decrease) {
    if (task->op == IOTask::READ) {
        if (task->offset >= 0) {
            task->res =
                    ::preadv(task->fd, task->iov, task->iovcnt, task->offset);
        } else {
            task->res = ::readv(task->fd, task->iov, task->iovcnt);
        }
    } else if (task->op == IOTask::WRITE) {
        if (task->offset >= 0) {
            task->res =
                    ::pwritev(task->fd, task->iov, task->iovcnt, task->offset);
        } else {
            task->res = ::writev(task->fd, task->iov, task->iovcnt);
        }
    } else if (task->op == IOTask::SYNC) {
        task->res = braft::raft_fsync(task->fd);
    } else {
        CHECK(false) << "unexpected, op: " << static_cast<int>(task->op);
    }

    task->error = errno;

    if (decrease) {
        inflight_bthread_io.fetch_sub(1, std::memory_order_relaxed);
    }

    task->butex->store(1, std::memory_order_release);

    if (wake) {
        int rc = bthread::butex_wake(task->butex);
        CHECK(rc == 0 || rc == 1);
    }

    task->Release();
}

void IOProxy::Run() {
    SetThreadName();

    moodycamel::ConsumerToken token{task_q_};

    while (true) {
        IOTask* task = nullptr;
        task_q_.wait_dequeue(token, task);

        if (CURVE_UNLIKELY(task->fd == IOTask::kStopTaskFd)) {
            break;
        }

        ExecuteTask(task, /*wake=*/true);
    }
}

}  // namespace chunkserver
}  // namespace curve
