/*
 *  Copyright (c) 2022 NetEase Inc.
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
 * Date: Monday Jun 06 17:38:05 CST 2022
 * Author: wuhanqing
 */

#include "src/mds/nameserver2/file_writer_lock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "proto/nameserver2.pb.h"
#include "src/common/timeutility.h"
#include "test/mds/mock/mock_etcdclient.h"

namespace curve {
namespace mds {

using ::testing::_;
using ::testing::Invoke;

using ::curve::common::TimeUtility;

class FileWriterLockManagerTest : public ::testing::Test {
 protected:
    void SetUp() override {
        opts_.ttlUs = 1000;

        etcdclient_ = std::make_shared<MockEtcdClient>();
        writerLock_.reset(new FileWriterLockManager(opts_, etcdclient_));
    }

 protected:
    FileWriterLockOptions opts_;
    std::shared_ptr<MockEtcdClient> etcdclient_;
    std::unique_ptr<FileWriterLockManager> writerLock_;
};

namespace {

struct FakeLock {
    std::string owner;
    uint64_t expiredUs;
    bool exist;

    FakeLock() : exist(false) {}

    FakeLock(std::string owner, uint64_t expiredUs)
        : owner(std::move(owner)), expiredUs(expiredUs), exist(true) {}

    int operator()(const std::string& /*key*/, std::string* value) const {
        if (!exist) {
            return EtcdErrCode::EtcdKeyNotExist;
        }

        FileWriterLock lock;
        lock.set_owner(owner);
        lock.set_expiredus(expiredUs);

        lock.SerializeToString(value);
        return EtcdErrCode::EtcdOK;
    }
};

}  // namespace

TEST_F(FileWriterLockManagerTest, TestLockIsHold) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(
            FakeLock{"alice", TimeUtility::GetTimeofDayUs() +
                                 60 * TimeUtility::MicroSecondsPerSecond}));

    ASSERT_FALSE(writerLock_->Lock(1, "bob"));
}

TEST_F(FileWriterLockManagerTest, TestReentrantLock) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(
            FakeLock{"alice", TimeUtility::GetTimeofDayUs() +
                                 60 * TimeUtility::MicroSecondsPerSecond}));
    EXPECT_CALL(*etcdclient_, Put(_, _))
        .Times(1);

    ASSERT_TRUE(writerLock_->Lock(1, "alice"));
}

TEST_F(FileWriterLockManagerTest, TestPreviousLockTimeout) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(FakeLock{"alice", TimeUtility::GetTimeofDayUs()}));
    EXPECT_CALL(*etcdclient_, Put(_, _))
        .Times(1);

    std::this_thread::sleep_for(std::chrono::microseconds(100));
    ASSERT_TRUE(writerLock_->Lock(1, "bob"));
}

TEST_F(FileWriterLockManagerTest, TestFirstLock) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(FakeLock{}));
    EXPECT_CALL(*etcdclient_, Put(_, _))
        .Times(1);

    ASSERT_TRUE(writerLock_->Lock(1, "bob"));
}

TEST_F(FileWriterLockManagerTest, TestUnlock) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(
            FakeLock{"alice", TimeUtility::GetTimeofDayUs() +
                                 60 * TimeUtility::MicroSecondsPerSecond}));
    EXPECT_CALL(*etcdclient_, Delete(_))
        .Times(1);

    ASSERT_TRUE(writerLock_->Unlock(1, "alice"));
}

TEST_F(FileWriterLockManagerTest, TestUnlockExpiredLock) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(
            FakeLock{"alice", TimeUtility::GetTimeofDayUs()}));
    EXPECT_CALL(*etcdclient_, Delete(_))
        .Times(1);

    std::this_thread::sleep_for(std::chrono::microseconds(100));
    ASSERT_TRUE(writerLock_->Unlock(1, "alice"));
}

TEST_F(FileWriterLockManagerTest, TestUnlockNotExist) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(
            FakeLock{}));

    ASSERT_FALSE(writerLock_->Unlock(1, "alice"));
}

TEST_F(FileWriterLockManagerTest, TestUnlockOwnerIsNotIdentical) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(
            FakeLock{"alice", TimeUtility::GetTimeofDayUs() +
                                 60 * TimeUtility::MicroSecondsPerSecond}));

    ASSERT_FALSE(writerLock_->Unlock(1, "bob"));
}

TEST_F(FileWriterLockManagerTest, TestUpdateLock) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(
            FakeLock{"alice", TimeUtility::GetTimeofDayUs() +
                                 60 * TimeUtility::MicroSecondsPerSecond}));
    EXPECT_CALL(*etcdclient_, Put(_, _))
        .Times(1);

    ASSERT_TRUE(writerLock_->Update(1, "alice"));
}

TEST_F(FileWriterLockManagerTest, TestUpdateExpiredLock) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(FakeLock{"alice", TimeUtility::GetTimeofDayUs()}));

    EXPECT_CALL(*etcdclient_, Put(_, _))
        .Times(1);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    ASSERT_TRUE(writerLock_->Update(1, "alice"));
}

TEST_F(FileWriterLockManagerTest, TestUpdateLockOwnerIsNotIdentical) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(
            FakeLock{"alice", TimeUtility::GetTimeofDayUs() +
                                  60 * TimeUtility::MicroSecondsPerSecond}));

    ASSERT_FALSE(writerLock_->Update(1, "bob"));
}

TEST_F(FileWriterLockManagerTest, TestUpdateLockOwnerIsNotIdenticalAndExpired) {
    EXPECT_CALL(*etcdclient_, Get(_, _))
        .WillOnce(Invoke(
            FakeLock{"alice", TimeUtility::GetTimeofDayUs()}));

    std::this_thread::sleep_for(std::chrono::microseconds(100));
    ASSERT_FALSE(writerLock_->Update(1, "bob"));
}

}  // namespace mds
}  // namespace curve
