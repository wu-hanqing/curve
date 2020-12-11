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

#include "src/client/discard_task.h"

namespace curve {
namespace client {

std::atomic<uint64_t> DiscardTask::taskId_(1);

bool DiscardTask::OnTriggeringTask(timespec*) {
    const FInfo* fileInfo = metaCache_->GetFileInfo();
    FileSegmentInfo* fileSegment =
        metaCache_->GetFileSegmentInfo(segmentIndex_);
    uint64_t offset =
        static_cast<uint64_t>(segmentIndex_) * fileInfo->segmentsize;

    FileSegmentWriteLockGuard lk(fileSegment);

    if (!fileSegment->IsAllDiscard()) {
        LOG(INFO) << "DiscardTask find bitmap was cleared, cancel task, "
                     "filename = "
                  << fileInfo->fullPathName << ", offset = " << offset
                  << ", taskid = " << id_;
        return false;
    }

    LIBCURVE_ERROR errCode = mdsClient_->DeAllocateSegment(fileInfo, offset);
    if (errCode == LIBCURVE_ERROR::OK) {
        fileSegment->ClearSegment();
        LOG(INFO) << "DiscardTask success, "
                  << ", filename = " << fileInfo->fullPathName
                  << ", offset = " << offset << ", taskid = " << id_;
    } else {
        LOG(ERROR) << "DiscardTask failed, error = " << errCode
                   << ", filename = " << fileInfo->fullPathName
                   << ", offset = " << offset << ", taskid = " << id_;
    }

    return false;
}

}  // namespace client
}  // namespace curve
