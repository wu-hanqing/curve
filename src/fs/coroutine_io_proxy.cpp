#include "src/fs/coroutine_io_proxy.h"

namespace curve {
namespace fs {

scoped_refptr<IOTask> IOTask::Create(int fd,
                                     Op op,
                                     off_t offset,
                                     int niov,
                                     struct iovec* iov,
                                     bool dma) {
    scoped_refptr<IOTask> task(new IOTask{});
    task->fd = fd;
    task->op = op;
    task->offset = offset;
    task->niov = niov;
    task->iov = iov;
    task->dma = dma;

    task->res = -1;
    task->butex = bthread::butex_create_checked<int>();
    CHECK(task->butex);
    *task->butex = 0;

    return task;
}

scoped_refptr<IOTask> IOTask::Create(int fd, Op op) {
    scoped_refptr<IOTask> task(new IOTask());
    task->fd = fd;
    task->op = op;
    task->res = -1;
    task->butex = bthread::butex_create_checked<butil::atomic<int>>();
    CHECK(task->butex);
    task->butex->store(0, butil::memory_order_relaxed);

    return task;
}

IOTask::~IOTask() {
    bthread::butex_destroy(butex);
}

ssize_t IOTask::Wait() {
    while (butex->load(butil::memory_order_acquire) != 1) {
        bthread::butex_wait(butex, 0, nullptr);
    }

    return res;
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

bool IOProxy::init() {
    for (uint32_t i = 0; i < FLAGS_io_worker_num; ++i) {
        workers_.emplace_back(&IOProxy::run, this);
    }

    running_.store(true);
    return true;
}

void IODelegate::enqueue(IOTask* task) {
    task->AddRef();
    if (bthread_self() != INVALID_BTHREAD &&
        inflight_bthread_io.fetch_add(1, std::memory_order_relaxed) >
                FLAGS_bthread_io_num) {
        inflight_bthread_io.fetch_sub(1, std::memory_order_relaxed);
        thread_local moodycamel::ProducerToken token{task_q_};

        // LOG(INFO) << "enqueue task: " << task << ", pid: " << pthread_self()
        //           << ", bthread id: " << bthread_self();
        if (task_q_.enqueue(token, task)) {
            return;
        }
    }

    // LOG(INFO) << "execute task immediately, task: " << task;
    execute_task(task, /*wake=*/false, /*decrease=*/true);
}

using AppendFunc = ssize_t (*)(int fd, const struct iovec* iov, int niov);

using OverWriteFunc = ssize_t (*)(int fd,
                                  const struct iovec* iov,
                                  int niov,
                                  off_t offset);

AppendFunc xxx1(IOTask* task) {
    if (task->op == IOTask::READ) {
        return task->dma ? pfs_readv_dma : pfs_readv;
    } else if (task->op == IOTask::WRITE) {
        return task->dma ? pfs_writev_dma : pfs_writev;
    }

    return nullptr;
}

OverWriteFunc xxx2(IOTask* task) {
    if (task->op == IOTask::READ) {
        return task->dma ? pfs_preadv_dma : pfs_preadv;
    } else if (task->op == IOTask::WRITE) {
        return task->dma ? pfs_pwritev_dma : pfs_pwritev;
    }

    return nullptr;
}

void IOProxy::execute_task(IOTask* task, bool wake, bool decrease) {
    // LOG(INFO) << "execute task, task: " << task;

    switch (task->op) {
        case IOTask::SYNC:
            task->res = pfs_fsync(fd);
            task->error = errno;
            break;
        case IOTask::READ:
        case IOTask::WRITE: {
            if (task->offset >= 0) {
                // pwritev/preadv
                auto fn = xxx1(task);
                task->res = fn(task->fd, task->iov, task->niov);
                task->error = errno;
            } else {
                // writev/readv
                auto fn = xxx2(task);
                task->res = fn(task->fd, task->iov, task->niov, task->offset);
                task->error = errno;
            }
            break;
        }
        default:
            CHECK(false) << "unexpected, op: " << static_cast<int>(task->op);
    }

//     if (task->op == IOTask::READ) {
//         if (task->offset >= 0) {
//             task->res = ::preadv(task->fd, task->iov, task->niov, task->offset);
//         } else {
//             task->res = ::readv(task->fd, task->iov, task->niov);
//         }
//     } else if (task->op == IOTask::WRITE) {
//         if (task->offset >= 0) {
//             task->res =
//                     ::pwritev(task->fd, task->iov, task->niov, task->offset);
//         } else {
//             task->res = ::writev(task->fd, task->iov, task->niov);
//         }
//     } else {
//         CHECK(false) << "unexpected, op: " << static_cast<int>(task->op);
//     }

// #ifndef NDEBUG
//     ssize_t sz = 0;
//     for (int i = 0; i < task->niov; ++i) {
//         sz += task->iov[i].iov_len;
//     }

//     if (sz != task->res) {
//         LOG(ERROR) << "read/write error, " << berror();
//     }
// #endif

    if (decrease) {
        inflight_bthread_io.fetch_sub(1, std::memory_order_relaxed);
    }

    // *task->butex = 1;
    task->butex->store(1, butil::memory_order_release);

    if (wake) {
        // LOG(INFO) << "execute task finished, task: " << task
        //           << ", res: " << task->res;
        int rc = bthread::butex_wake(task->butex);  // 需要保证这里对象存在
        CHECK(rc == 0 || rc == 1);
    }

    task->Release();
}

static void set_thread_name() {
    static std::atomic<int> count{1};
    int id = count.fetch_add(1, std::memory_order_relaxed);
    std::string name = "io-" + std::to_string(id);
    pthread_setname_np(pthread_self(), name.c_str());
}

void IODelegate::run() {
    set_thread_name();

    moodycamel::ConsumerToken token{task_q_};

    while (true) {
        IOTask* task = nullptr;
        task_q_.wait_dequeue(token, task);

        if (CURVE_UNLIKELY(task->fd == IOTask::kStopTaskFd)) {
            break;
        }

        execute_task(task, /*wake=*/true);

        // if (task.op == IOTask::READ) {
        //     if (task.offset >= 0) {
        //         task.res = ::preadv(task.fd, task.iov, task.niov,
        //         task.offset);
        //     } else {
        //         task.res = ::readv(task.fd, task.iov, task.niov);
        //     }
        // } else if (task.op == IOTask::WRITE) {
        //     if (task.offset >= 0) {
        //         task.res = ::pwritev(task.fd, task.iov, task.niov,
        //         task.offset);
        //     } else {
        //         task.res = ::writev(task.fd, task.iov, task.niov);
        //     }
        // } else {
        //     CHECK(false) << "unexpected, op: " << static_cast<int>(task.op);
        // }

        // *task.butex = 1;
        // bthread::butex_wake(task.butex);
    }
}

}  // namespace fs
}  // namespace curve
