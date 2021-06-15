/*
 *  Copyright (c) 2021 NetEase Inc.
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

#include "curvefs/src/space/space_manager.h"

#include <gtest/gtest.h>

#include "curvefs/src/space/utils.h"
#include "curvefs/test/space/common.h"

namespace curvefs {
namespace space {

class SpaceManagerTest : public ::testing::Test {
 protected:
    void SetUp() override {
        space_.reset(new SpaceManagerImpl({}));
    }

    void TearDown() override {}

    void PrepareEnv() {
        mds::FsInfo blockFsInfo;
        blockFsInfo.set_fsid(1);
        blockFsInfo.set_fsname("block");
        blockFsInfo.set_fstype(mds::FSType::TYPE_VOLUME);
        blockFsInfo.set_capacity(10 * kGiB);
        EXPECT_EQ(SPACE_OK, space_->InitSpace(blockFsInfo));

        mds::FsInfo s3FsInfo;
        s3FsInfo.set_fsid(2);
        s3FsInfo.set_fsname("s3");
        s3FsInfo.set_fstype(mds::FSType::TYPE_S3);
        EXPECT_EQ(SPACE_OK, space_->InitSpace(s3FsInfo));
    }

 protected:
    std::unique_ptr<SpaceManager> space_;
};

TEST_F(SpaceManagerTest, TestInitSpace) {
    mds::FsInfo blockFsInfo;
    blockFsInfo.set_fsid(1);
    blockFsInfo.set_fsname("block");
    blockFsInfo.set_fstype(mds::FSType::TYPE_VOLUME);
    EXPECT_EQ(SPACE_OK, space_->InitSpace(blockFsInfo));

    mds::FsInfo s3FsInfo;
    s3FsInfo.set_fsid(1);
    s3FsInfo.set_fsname("s3");
    s3FsInfo.set_fstype(mds::FSType::TYPE_S3);
    EXPECT_EQ(SPACE_OK, space_->InitSpace(s3FsInfo));

    // if fsid is already exists, init return exists
    EXPECT_EQ(SPACE_EXISTS, space_->InitSpace(blockFsInfo));
    EXPECT_EQ(SPACE_EXISTS, space_->InitSpace(s3FsInfo));
}

TEST_F(SpaceManagerTest, TestUnInitSpace) {
    EXPECT_EQ(SPACE_NOT_FOUND,
              space_->UnInitSpace(1, mds::FSType::TYPE_VOLUME));
    EXPECT_EQ(SPACE_NOT_FOUND,
              space_->UnInitSpace(2, mds::FSType::TYPE_VOLUME));
    EXPECT_EQ(SPACE_NOT_FOUND, space_->UnInitSpace(1, mds::FSType::TYPE_S3));
    EXPECT_EQ(SPACE_NOT_FOUND, space_->UnInitSpace(2, mds::FSType::TYPE_S3));

    PrepareEnv();

    EXPECT_EQ(SPACE_OK, space_->UnInitSpace(1, mds::FSType::TYPE_VOLUME));
    EXPECT_EQ(SPACE_OK, space_->UnInitSpace(2, mds::FSType::TYPE_S3));

    EXPECT_EQ(SPACE_NOT_FOUND,
              space_->UnInitSpace(1, mds::FSType::TYPE_VOLUME));
    EXPECT_EQ(SPACE_NOT_FOUND, space_->UnInitSpace(2, mds::FSType::TYPE_S3));
}

TEST_F(SpaceManagerTest, TestAllocateSpace) {
    PrepareEnv();

    Extents exts;
    EXPECT_EQ(SPACE_NOT_FOUND, space_->AllocateSpace(100, 1 * kGiB, {}, &exts));
    EXPECT_TRUE(exts.empty());

    EXPECT_EQ(SPACE_OK, space_->AllocateSpace(1, 1 * kGiB, {}, &exts));
    EXPECT_EQ(1 * kGiB, TotalLength(exts));

    Extents exts2;
    EXPECT_EQ(SPACE_NOSPACE, space_->AllocateSpace(1, 10 * kGiB, {}, &exts2));
    EXPECT_TRUE(exts2.empty());

    uint64_t total = 0;
    uint64_t avail = 0;
    uint64_t blkSize = 0;
    EXPECT_EQ(SPACE_OK, space_->StatSpace(1, &total, &avail, &blkSize));
    EXPECT_EQ(9 * kGiB, avail);
}

TEST_F(SpaceManagerTest, TestDeallocateSpace) {
    PrepareEnv();

    EXPECT_EQ(SPACE_NOT_FOUND, space_->DeallocateSpace(100, {}));

    Extents exts;
    EXPECT_EQ(SPACE_OK, space_->AllocateSpace(1, 5 * kGiB, {}, &exts));
    EXPECT_EQ(5 * kGiB, TotalLength(exts));

    EXPECT_EQ(SPACE_OK,
              space_->DeallocateSpace(1, ConvertToProtoExtents(exts)));

    uint64_t total = 0;
    uint64_t avail = 0;
    uint64_t blkSize = 0;
    EXPECT_EQ(SPACE_OK, space_->StatSpace(1, &total, &avail, &blkSize));
    EXPECT_EQ(total, avail);
    EXPECT_EQ(10 * kGiB, avail);

    Extents dealloc(exts);
    EXPECT_EQ(SPACE_DEALLOC_ERROR,
              space_->DeallocateSpace(1, ConvertToProtoExtents(exts)));
}

TEST_F(SpaceManagerTest, TestAllocateS3Chunk) {
    PrepareEnv();

    uint64_t chunkId = 0;
    EXPECT_EQ(SPACE_NOT_FOUND, space_->AllocateS3Chunk(100, &chunkId));

    EXPECT_EQ(SPACE_OK, space_->AllocateS3Chunk(2, &chunkId));
}

TEST_F(SpaceManagerTest, TestStatSpace) {
    PrepareEnv();

    uint64_t total = 0;
    uint64_t avail = 0;
    uint64_t blkSize = 0;
    EXPECT_EQ(SPACE_NOT_FOUND,
              space_->StatSpace(100, &total, &avail, &blkSize));

    EXPECT_EQ(SPACE_OK,
              space_->StatSpace(1, &total, &avail, &blkSize));
    EXPECT_EQ(total, avail);
    EXPECT_EQ(total, 10 * kGiB);
}

}  // namespace space
}  // namespace curvefs

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
