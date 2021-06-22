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
 * Date: Tue Jun 22 19:26:16 CST 2021
 * Author: wuhanqing
 */

#include <gtest/gtest.h>

#include "src/client/io_tracker.h"
#include "src/client/splitor.h"
#include "test/client/mock/mock_mdsclient.h"
#include "test/client/mock/mock_meta_cache.h"
#include "test/client/mock/mock_request_scheduler.h"

namespace curve {
namespace client {

using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Matcher;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::Invoke;

class IOTrackerAlignmentTest : public ::testing::Test {
    void SetUp() override {
        Splitor::Init(splitOpt_);

        mockMDSClient_.reset(new MockMDSClient());
        mockMetaCache_.reset(new MockMetaCache());
        mockScheduler_.reset(new MockRequestScheduler());

        MetaCacheOption metaCacheOpt;
        mockMetaCache_->Init(metaCacheOpt, mockMDSClient_.get());

        fileInfo_.fullPathName = "/IOTrackerAlignmentTest";
        fileInfo_.length = 100 * GiB;
        fileInfo_.segmentsize = 1 * GiB;
        fileInfo_.chunksize = 16 * MiB;
        fileInfo_.filestatus = FileStatus::CloneMetaInstalled;

        mockMetaCache_->UpdateFileInfo(fileInfo_);
    }

    void TearDown() override {
    }

 protected:
    FInfo fileInfo_;
    std::unique_ptr<DiscardMetric> metric;
    std::unique_ptr<MockMetaCache> mockMetaCache_;
    std::unique_ptr<MockMDSClient> mockMDSClient_;
    std::unique_ptr<MockRequestScheduler> mockScheduler_;

    IOSplitOption splitOpt_;
};

TEST_F(IOTrackerAlignmentTest, TestUnalignedWrite1) {
    std::vector<RequestContext*> requests;
    std::mutex mtx;

    auto saver = [&requests, &mtx](const std::vector<RequestContext*>& reqs) {
        std::lock_guard<std::mutex> lock(mtx);
        requests.insert(requests.end(), reqs.begin(), reqs.end());

        return 0;
    };

    EXPECT_CALL(*mockMetaCache_, GetChunkInfoByIndex(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([](ChunkIndex idx, ChunkIDInfo_t* info) {
            info->chunkExist = true;
            info->lpid_ = 1;
            info->cpid_ = 2;
            info->cid_ = 3;

            return MetaCacheErrorType::OK;
        }));
    EXPECT_CALL(*mockScheduler_, ScheduleRequest(Matcher<const std::vector<RequestContext*>&>(_)))  // NOLINT
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(saver));
    EXPECT_CALL(*mockScheduler_, ScheduleRequest(Matcher<RequestContext*>(_)))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([&requests, &mtx](RequestContext* req) {
            std::lock_guard<std::mutex> lock(mtx);
            requests.push_back(req);
            return 0;
        }));

    IOTracker tracker(nullptr, mockMetaCache_.get(), mockScheduler_.get());

    butil::IOBuf fakeData;
    fakeData.resize(2048, 'b');
    tracker.SetUserDataType(UserDataType::IOBuffer);
    tracker.StartWrite(
        &fakeData, 0, 2048, mockMDSClient_.get(), &fileInfo_);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(1, requests.size());
    auto* r1 = requests[0];

    EXPECT_EQ(OpType::READ, r1->optype_);
    EXPECT_EQ(0, r1->offset_);
    EXPECT_EQ(splitOpt_.alignment.clone, r1->rawlength_);
    EXPECT_NE(nullptr, dynamic_cast<PaddingReadClosure*>(r1->done_));

    // set this padding read success
    r1->done_->SetFailed(0);
    r1->readData_.resize(r1->rawlength_, 'a');
    r1->done_->Run();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(2, requests.size());
    auto* r2 = requests[1];
    EXPECT_EQ(OpType::WRITE, r2->optype_);
    EXPECT_EQ(0, r2->offset_);
    EXPECT_EQ(splitOpt_.alignment.clone, r2->rawlength_);
    EXPECT_EQ(nullptr, dynamic_cast<PaddingReadClosure*>(r2->done_));

    butil::IOBuf expectedWriteData;
    expectedWriteData.resize(2048, 'b');
    expectedWriteData.resize(4096, 'a');
    EXPECT_EQ(r2->writeData_, expectedWriteData);
}

TEST_F(IOTrackerAlignmentTest, TestUnalignedWrite2) {
    std::vector<RequestContext*> requests;
    std::mutex mtx;

    auto saver = [&requests, &mtx](const std::vector<RequestContext*>& reqs) {
        std::lock_guard<std::mutex> lock(mtx);
        requests.insert(requests.end(), reqs.begin(), reqs.end());

        return 0;
    };

    EXPECT_CALL(*mockMetaCache_, GetChunkInfoByIndex(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([](ChunkIndex idx, ChunkIDInfo_t* info) {
            info->chunkExist = true;
            info->lpid_ = 1;
            info->cpid_ = 2;
            info->cid_ = 3;

            return MetaCacheErrorType::OK;
        }));
    EXPECT_CALL(*mockScheduler_, ScheduleRequest(Matcher<const std::vector<RequestContext*>&>(_)))  // NOLINT
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(saver));
    EXPECT_CALL(*mockScheduler_, ScheduleRequest(Matcher<RequestContext*>(_)))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([&requests, &mtx](RequestContext* req) {
            std::lock_guard<std::mutex> lock(mtx);
            requests.push_back(req);
            return 0;
        }));

    IOTracker tracker(nullptr, mockMetaCache_.get(), mockScheduler_.get());

    butil::IOBuf fakeData;
    uint64_t offset = 0;
    uint64_t length = 64 * KiB - 1024;
    fakeData.resize(length, 'b');
    tracker.SetUserDataType(UserDataType::IOBuffer);
    tracker.StartWrite(&fakeData, offset, length, mockMDSClient_.get(),
                       &fileInfo_);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(2, requests.size());
    auto* r1 = requests[0];

    EXPECT_EQ(OpType::WRITE, r1->optype_);
    EXPECT_EQ(0, r1->offset_);
    EXPECT_EQ(60 * KiB, r1->rawlength_);
    EXPECT_EQ(nullptr, dynamic_cast<PaddingReadClosure*>(r1->done_));

    auto* r2 = requests[1];
    EXPECT_EQ(OpType::READ, r2->optype_);
    EXPECT_EQ(60 * KiB, r2->offset_);
    EXPECT_EQ(splitOpt_.alignment.clone, r2->rawlength_);
    EXPECT_NE(nullptr, dynamic_cast<PaddingReadClosure*>(r2->done_));

    // set this padding read success
    r2->done_->SetFailed(0);
    r2->readData_.resize(r2->rawlength_, 'a');
    r2->done_->Run();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(3, requests.size());
    auto* r3 = requests[2];
    EXPECT_EQ(OpType::WRITE, r3->optype_);
    EXPECT_EQ(60 * KiB, r3->offset_);
    EXPECT_EQ(splitOpt_.alignment.clone, r3->rawlength_);
    EXPECT_EQ(nullptr, dynamic_cast<PaddingReadClosure*>(r3->done_));

    butil::IOBuf expectedWriteData;
    expectedWriteData.resize(3072, 'b');
    expectedWriteData.resize(4096, 'a');
    EXPECT_EQ(r3->writeData_, expectedWriteData);
}

TEST_F(IOTrackerAlignmentTest, TestUnalignedWrite3) {
    std::vector<RequestContext*> requests;
    std::mutex mtx;

    auto saver = [&requests, &mtx](const std::vector<RequestContext*>& reqs) {
        std::lock_guard<std::mutex> lock(mtx);
        requests.insert(requests.end(), reqs.begin(), reqs.end());

        return 0;
    };

    EXPECT_CALL(*mockMetaCache_, GetChunkInfoByIndex(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([](ChunkIndex idx, ChunkIDInfo_t* info) {
            info->chunkExist = true;
            info->lpid_ = 1;
            info->cpid_ = 2;
            info->cid_ = 3;

            return MetaCacheErrorType::OK;
        }));
    EXPECT_CALL(*mockScheduler_, ScheduleRequest(Matcher<const std::vector<RequestContext*>&>(_)))  // NOLINT
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(saver));
    EXPECT_CALL(*mockScheduler_, ScheduleRequest(Matcher<RequestContext*>(_)))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([&requests, &mtx](RequestContext* req) {
            std::lock_guard<std::mutex> lock(mtx);
            requests.push_back(req);
            return 0;
        }));

    IOTracker tracker(nullptr, mockMetaCache_.get(), mockScheduler_.get());

    butil::IOBuf fakeData;
    fakeData.resize(2048, 'b');
    tracker.SetUserDataType(UserDataType::IOBuffer);
    tracker.StartWrite(
        &fakeData, 2048, 2048, mockMDSClient_.get(), &fileInfo_);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(1, requests.size());
    auto* r1 = requests[0];

    EXPECT_EQ(OpType::READ, r1->optype_);
    EXPECT_EQ(0, r1->offset_);
    EXPECT_EQ(splitOpt_.alignment.clone, r1->rawlength_);
    EXPECT_NE(nullptr, dynamic_cast<PaddingReadClosure*>(r1->done_));

    // set this padding read success
    r1->done_->SetFailed(0);
    r1->readData_.resize(r1->rawlength_, 'a');
    r1->done_->Run();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(2, requests.size());
    auto* r2 = requests[1];
    EXPECT_EQ(OpType::WRITE, r2->optype_);
    EXPECT_EQ(0, r2->offset_);
    EXPECT_EQ(splitOpt_.alignment.clone, r2->rawlength_);
    EXPECT_EQ(nullptr, dynamic_cast<PaddingReadClosure*>(r2->done_));

    butil::IOBuf expectedWriteData;
    expectedWriteData.resize(2048, 'a');
    expectedWriteData.resize(4096, 'b');
    EXPECT_EQ(r2->writeData_, expectedWriteData);
}

TEST_F(IOTrackerAlignmentTest, TestUnalignedWrite4) {
    std::vector<RequestContext*> requests;
    std::mutex mtx;

    auto saver = [&requests, &mtx](const std::vector<RequestContext*>& reqs) {
        std::lock_guard<std::mutex> lock(mtx);
        requests.insert(requests.end(), reqs.begin(), reqs.end());

        return 0;
    };

    EXPECT_CALL(*mockMetaCache_, GetChunkInfoByIndex(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([](ChunkIndex idx, ChunkIDInfo_t* info) {
            info->chunkExist = true;
            info->lpid_ = 1;
            info->cpid_ = 2;
            info->cid_ = 3;

            return MetaCacheErrorType::OK;
        }));
    EXPECT_CALL(*mockScheduler_, ScheduleRequest(Matcher<const std::vector<RequestContext*>&>(_)))  // NOLINT
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(saver));
    EXPECT_CALL(*mockScheduler_, ScheduleRequest(Matcher<RequestContext*>(_)))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke([&requests, &mtx](RequestContext* req) {
            std::lock_guard<std::mutex> lock(mtx);
            requests.push_back(req);
            return 0;
        }));

    IOTracker tracker(nullptr, mockMetaCache_.get(), mockScheduler_.get());

    butil::IOBuf fakeData;
    uint64_t offset = 1024;
    uint64_t length = 64 * KiB - 1024;
    fakeData.resize(length, 'b');
    tracker.SetUserDataType(UserDataType::IOBuffer);
    tracker.StartWrite(&fakeData, offset, length, mockMDSClient_.get(),
                       &fileInfo_);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(2, requests.size());
    auto* r1 = !requests[0]->aligned ? requests[0] : requests[1];

    EXPECT_EQ(OpType::READ, r1->optype_);
    EXPECT_EQ(0, r1->offset_);
    EXPECT_EQ(splitOpt_.alignment.clone, r1->rawlength_);
    EXPECT_NE(nullptr, dynamic_cast<PaddingReadClosure*>(r1->done_));

    auto* r2 = requests[1]->aligned ? requests[1] : requests[0];
    EXPECT_EQ(OpType::WRITE, r2->optype_);
    EXPECT_EQ(splitOpt_.alignment.clone, r2->offset_);
    EXPECT_EQ(60 * KiB, r2->rawlength_);
    EXPECT_EQ(nullptr, dynamic_cast<PaddingReadClosure*>(r2->done_));

    // set this padding read success
    r1->done_->SetFailed(0);
    r1->readData_.resize(r1->rawlength_, 'a');
    r1->done_->Run();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(3, requests.size());
    auto* r3 = requests[2];
    EXPECT_EQ(OpType::WRITE, r3->optype_);
    EXPECT_EQ(0, r3->offset_);
    EXPECT_EQ(splitOpt_.alignment.clone, r3->rawlength_);
    EXPECT_EQ(nullptr, dynamic_cast<PaddingReadClosure*>(r3->done_));

    butil::IOBuf expectedWriteData;
    expectedWriteData.resize(1024, 'a');
    expectedWriteData.resize(4096, 'b');
    EXPECT_EQ(r3->writeData_, expectedWriteData);
}

}  // namespace client
}  // namespace curve
