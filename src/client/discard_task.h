/*
 *  Copyright (c) 2020 NetEase Inc.
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

/*
 * Project: curve
 * File Created: Thu Dec 17 11:05:38 CST 2020
 * Author: wuhanqing
 */

#ifndef SRC_CLIENT_DISCARD_TASK_H_
#define SRC_CLIENT_DISCARD_TASK_H_

#include <time.h>
#include <brpc/periodic_task.h>

#include <atomic>

#include "src/client/metacache.h"
#include "src/client/metacache_struct.h"

namespace curve {
namespace client {

class DiscardTask : public brpc::PeriodicTask {
 public:
    DiscardTask(SegmentIndex segmentIndex, MetaCache* metaCache,
                MDSClient* mdsClient)
        : segmentIndex_(segmentIndex),
          metaCache_(metaCache),
          mdsClient_(mdsClient),
          id_(GetNextTaskId()) {}

    bool OnTriggeringTask(timespec*) override;

    void OnDestroyingTask() override {
        delete this;
    }

    uint64_t Id() const {
        return id_;
    }

 private:
    static uint64_t GetNextTaskId() {
        return taskId_.fetch_add(1, std::memory_order_relaxed);
    }

    SegmentIndex segmentIndex_;
    MetaCache* metaCache_;
    MDSClient* mdsClient_;
    uint64_t id_;

    static std::atomic<uint64_t> taskId_;
};

}  // namespace client
}  // namespace curve

#endif  // SRC_CLIENT_DISCARD_TASK_H_
