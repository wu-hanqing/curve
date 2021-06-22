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
 * File Created: Tuesday, 9th October 2018 7:03:23 pm
 * Author: tongguangxun
 */

#include "src/client/request_closure.h"

#include <brpc/closure_guard.h>

#include <memory>

#include "src/client/client_metric.h"
#include "src/client/inflight_controller.h"
#include "src/client/io_tracker.h"
#include "src/client/iomanager.h"
#include "src/client/request_context.h"
#include "src/client/request_scheduler.h"
#include "src/common/fast_align.h"

namespace curve {
namespace client {

void RequestClosure::Run() {
    ReleaseInflightRPCToken();
    if (suspendRPC_) {
        MetricHelper::DecremIOSuspendNum(metric_);
    }
    tracker_->HandleResponse(reqCtx_);
}

void RequestClosure::GetInflightRPCToken() {
    if (ioManager_ != nullptr) {
        ioManager_->GetInflightRpcToken();
        MetricHelper::IncremInflightRPC(metric_);
        ownInflight_ = true;
    }
}

void RequestClosure::ReleaseInflightRPCToken() {
    if (ioManager_ != nullptr && ownInflight_) {
        ioManager_->ReleaseInflightRpcToken();
        MetricHelper::DecremInflightRPC(metric_);
    }
}

PaddingReadClosure::PaddingReadClosure(RequestContext* requestCtx,
                                       RequestScheduler* scheduler,
                                       uint32_t align)
    : RequestClosure(requestCtx),
      align_(align),
      alignedCtx_(nullptr),
      scheduler_(scheduler) {
    GenAlignedRequest();
}

void PaddingReadClosure::Run() {
    LOG(INFO) << "in PaddingReadClosure::Run";

    std::unique_ptr<PaddingReadClosure> selfGuard(this);
    std::unique_ptr<RequestContext> ctxGuard(alignedCtx_);

    const int errCode = GetErrorCode();
    if (errCode != 0) {
        HandleError(errCode);
        return;
    }

    switch (reqCtx_->optype_) {
        case OpType::READ:
            HandleRead();
            break;
        case OpType::WRITE:
            HandleWrite();
            break;
        default:
            HandleError(-1);
            LOG(ERROR) << "unexpected optype, request context: " << *reqCtx_;
    }
}

void PaddingReadClosure::HandleRead() {
    brpc::ClosureGuard doneGuard(reqCtx_->done_);

    // copy data to read request
    auto nc = alignedCtx_->readData_.append_to(
        &reqCtx_->readData_, reqCtx_->rawlength_,
        reqCtx_->offset_ - alignedCtx_->offset_);

    if (nc != reqCtx_->rawlength_) {
        LOG(FATAL) << "unexpected";
        reqCtx_->done_->SetFailed(-1);
    } else {
        reqCtx_->done_->SetFailed(0);
    }
}

void PaddingReadClosure::HandleWrite() {
    LOG(INFO) << "in PaddingReadClosure::HandleWrite()";

    // padding data
    butil::IOBuf alignedData;
    uint64_t n = 0;

    // pad left read data if necessary
    uint64_t bytes = reqCtx_->offset_ - alignedCtx_->offset_;
    uint64_t pos = 0;
    if (bytes != 0) {
        n = alignedCtx_->readData_.append_to(&alignedData, bytes);
        pos += bytes;
        LOG(INFO) << n;
    }

    // write data
    bytes = reqCtx_->rawlength_;
    n = reqCtx_->writeData_.append_to(&alignedData, bytes);
    pos += bytes;
    LOG(INFO) << n;

    // pad right read data if necessary
    bytes = alignedCtx_->rawlength_ - pos;
    if (bytes != 0) {
        n = alignedCtx_->readData_.append_to(&alignedData, bytes, pos);
        LOG(INFO) << n;
    }

    // issue a aligned request
    reqCtx_->offset_ = alignedCtx_->offset_;
    reqCtx_->rawlength_ = alignedCtx_->rawlength_;
    reqCtx_->writeData_.swap(alignedData);

    // TODO(wuhanqing): retcode process
    scheduler_->ScheduleRequest(reqCtx_);
}

void PaddingReadClosure::GenAlignedRequest() {
    alignedCtx_ = new RequestContext();
    alignedCtx_->idinfo_ = reqCtx_->idinfo_;
    alignedCtx_->optype_ = OpType::READ;  // set to read

    alignedCtx_->offset_ = common::align_down(reqCtx_->offset_, align_);
    alignedCtx_->rawlength_ = common::align_up(reqCtx_->rawlength_, align_);

    alignedCtx_->done_ = this;

    alignedCtx_->seq_ = reqCtx_->seq_;
    alignedCtx_->appliedindex_ = reqCtx_->appliedindex_;
    alignedCtx_->chunksize_ = reqCtx_->chunksize_;
    alignedCtx_->location_ = reqCtx_->location_;
    alignedCtx_->sourceInfo_ = reqCtx_->sourceInfo_;
}

void PaddingReadClosure::HandleError(int errCode) {
    brpc::ClosureGuard doneGuard(reqCtx_->done_);
    reqCtx_->done_->SetFailed(errCode);

    LOG(ERROR) << "Padding read request failed, request: " << *alignedCtx_
               << ", error: " << errCode;
}

}  // namespace client
}  // namespace curve
