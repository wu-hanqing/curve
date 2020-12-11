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

/**
 * Project: curve
 * Date: Tue Dec 15 20:38:06 CST 2020
 * Author: wuhanqing
 */

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "src/client/metacache_struct.h"

namespace curve {
namespace client {

constexpr uint64_t KiB = 1024;
constexpr uint64_t MiB = 1024 * KiB;
constexpr uint64_t GiB = 1024 * MiB;

TEST(TestFileSegmentInfo, TestDiscard) {
    const SegmentIndex segmentIndex = 0;
    const uint32_t segmentSize = 1 * GiB;

    const std::vector<uint32_t> discardGranularities{
        4 * KiB,   8 * KiB, 16 * KiB, 32 * KiB, 64 * KiB, 128 * KiB, 256 * KiB,
        512 * KiB, 1 * MiB, 2 * MiB,  4 * MiB,  8 * MiB,  16 * MiB};

    for (const auto& discardGranularity : discardGranularities) {
        LOG(INFO) << "discardGranularity: " << discardGranularity;

        // discard entire segment
        {
            FileSegmentInfo segment(segmentIndex, segmentSize,
                                    discardGranularity);

            segment.SetDiscard(0, segmentSize);
            ASSERT_EQ(Bitmap::NO_POS, segment.GetBitmap().NextClearBit(0));
        }

        // discard length smaller than discard granularity
        {
            FileSegmentInfo segment(segmentIndex, segmentSize,
                                    discardGranularity);

            segment.SetDiscard(0, discardGranularity - 1);
            ASSERT_EQ(Bitmap::NO_POS, segment.GetBitmap().NextSetBit(0));

            segment.SetDiscard(segmentSize - discardGranularity, 1);
            ASSERT_EQ(Bitmap::NO_POS, segment.GetBitmap().NextSetBit(0));
        }

        // discard offset and length are both align to discard granularity
        {
            FileSegmentInfo segment(segmentIndex, segmentSize,
                                    discardGranularity);

            segment.SetDiscard(0, discardGranularity);
            segment.SetDiscard(25 * discardGranularity, 4 * discardGranularity);
            segment.SetDiscard(segmentSize - 2 * discardGranularity,
                               2 * discardGranularity);

            std::vector<curve::common::BitRange> clearRanges;
            std::vector<curve::common::BitRange> setRanges;

            uint32_t endIndex = segmentSize / discardGranularity - 1;

            segment.GetBitmap().Divide(0, endIndex, &clearRanges, &setRanges);

            for (auto& r : setRanges) {
                LOG(INFO) << "begin: " << r.beginIndex
                          << " end: " << r.endIndex;
            }

            std::vector<curve::common::BitRange> expectedRanges{
                {0, 0}, {25, 28}, {endIndex - 1, endIndex}};

            ASSERT_EQ(setRanges.size(), expectedRanges.size());
            for (size_t i = 0; i < setRanges.size(); ++i) {
                ASSERT_EQ(setRanges[i].beginIndex,
                          expectedRanges[i].beginIndex);
                ASSERT_EQ(setRanges[i].endIndex, expectedRanges[i].endIndex);
            }
        }

        // discard offset or length aren't align to discard granularity
        {
            FileSegmentInfo segment(segmentIndex, segmentSize,
                                    discardGranularity);

            segment.SetDiscard(discardGranularity,
                               discardGranularity + 1);  // {1, 1}
            segment.SetDiscard(5 * discardGranularity - 1,
                               discardGranularity + 1);  // {5, 5}
            segment.SetDiscard(10 * discardGranularity + 1,
                               2 * discardGranularity - 2);  // not valid
            segment.SetDiscard(20 * discardGranularity + 1,
                               3 * discardGranularity - 2);  // {21, 21}
            segment.SetDiscard(30 * discardGranularity - 1,
                               discardGranularity + 2);  // {30, 30}

            std::vector<curve::common::BitRange> clearRanges;
            std::vector<curve::common::BitRange> setRanges;

            uint32_t endIndex = segmentSize / discardGranularity - 1;

            segment.GetBitmap().Divide(0, endIndex, &clearRanges, &setRanges);

            for (auto& r : setRanges) {
                LOG(INFO) << "begin: " << r.beginIndex
                          << " end: " << r.endIndex;
            }

            std::vector<curve::common::BitRange> expectedRanges{
                {1, 1}, {5, 5}, {21, 21}, {30, 30}};

            ASSERT_EQ(setRanges.size(), expectedRanges.size());
            for (size_t i = 0; i < setRanges.size(); ++i) {
                ASSERT_EQ(setRanges[i].beginIndex,
                          expectedRanges[i].beginIndex);
                ASSERT_EQ(setRanges[i].endIndex, expectedRanges[i].endIndex);
            }

            segment.SetDiscard(1, segmentSize - 2);
            clearRanges.clear();
            setRanges.clear();

            segment.GetBitmap().Divide(0, endIndex, &clearRanges, &setRanges);
            ASSERT_EQ(1, setRanges.size());
            ASSERT_EQ(1, setRanges.front().beginIndex);
            ASSERT_EQ(endIndex - 1, setRanges.front().endIndex);
        }
    }
}

TEST(TestFileSegmentInfo, TestClearDiscard) {
    const SegmentIndex segmentIndex = 0;
    const uint32_t segmentSize = 1 * GiB;

    const std::vector<uint32_t> discardGranularities{
        4 * KiB,   8 * KiB, 16 * KiB, 32 * KiB, 64 * KiB, 128 * KiB, 256 * KiB,
        512 * KiB, 1 * MiB, 2 * MiB,  4 * MiB,  8 * MiB,  16 * MiB};

    for (const auto& discardGranularity : discardGranularities) {
        LOG(INFO) << "discardGranularity: " << discardGranularity;

        // clear entire segment
        {
            FileSegmentInfo segment(segmentIndex, segmentSize,
                                    discardGranularity);
            segment.GetBitmap().Set();

            segment.ClearDiscard(0, segmentSize);
            ASSERT_EQ(Bitmap::NO_POS, segment.GetBitmap().NextSetBit(0));
        }

        // clear length smaller than discard granularity
        {
            FileSegmentInfo segment(segmentIndex, segmentSize,
                                    discardGranularity);
            segment.GetBitmap().Set();

            segment.ClearDiscard(0, discardGranularity - 1);
            ASSERT_FALSE(segment.GetBitmap().Test(0));

            segment.ClearDiscard(segmentSize - discardGranularity, 1);
            ASSERT_FALSE(
                segment.GetBitmap().Test(segmentSize / discardGranularity - 1));
        }

        // clear offset and length are both align to discard granularity
        {
            FileSegmentInfo segment(segmentIndex, segmentSize,
                                    discardGranularity);
            segment.GetBitmap().Set();

            segment.ClearDiscard(0, discardGranularity);
            segment.ClearDiscard(25 * discardGranularity,
                                 4 * discardGranularity);
            segment.ClearDiscard(segmentSize - 2 * discardGranularity,
                                 2 * discardGranularity);

            std::vector<curve::common::BitRange> clearRanges;
            std::vector<curve::common::BitRange> setRanges;

            uint32_t endIndex = segmentSize / discardGranularity - 1;

            segment.GetBitmap().Divide(0, endIndex, &clearRanges, &setRanges);

            for (auto& r : clearRanges) {
                LOG(INFO) << "begin: " << r.beginIndex
                          << " end: " << r.endIndex;
            }

            std::vector<curve::common::BitRange> expectedRanges{
                {0, 0}, {25, 28}, {endIndex - 1, endIndex}};

            ASSERT_EQ(clearRanges.size(), expectedRanges.size());
            for (size_t i = 0; i < clearRanges.size(); ++i) {
                ASSERT_EQ(clearRanges[i].beginIndex,
                          expectedRanges[i].beginIndex);
                ASSERT_EQ(clearRanges[i].endIndex, expectedRanges[i].endIndex);
            }
        }

        // clear offset or length aren't align to discard granularity
        {
            FileSegmentInfo segment(segmentIndex, segmentSize,
                                    discardGranularity);
            segment.GetBitmap().Set();

            segment.ClearDiscard(discardGranularity,
                                 discardGranularity + 1);  // {1, 2}
            segment.ClearDiscard(5 * discardGranularity - 1,
                                 discardGranularity + 1);  // {4, 5}
            segment.ClearDiscard(10 * discardGranularity + 1,
                                 2 * discardGranularity - 2);  // {10, 11}
            segment.ClearDiscard(20 * discardGranularity + 1,
                                 3 * discardGranularity - 2);  // {20, 22}
            segment.ClearDiscard(30 * discardGranularity - 1,
                                 discardGranularity + 2);  // {29, 31}

            std::vector<curve::common::BitRange> clearRanges;
            std::vector<curve::common::BitRange> setRanges;

            uint32_t endIndex = segmentSize / discardGranularity - 1;

            segment.GetBitmap().Divide(0, endIndex, &clearRanges, &setRanges);

            for (auto& r : clearRanges) {
                LOG(INFO) << "begin: " << r.beginIndex
                          << " end: " << r.endIndex;
            }

            std::vector<curve::common::BitRange> expectedRanges{
                {1, 2}, {4, 5}, {10, 11}, {20, 22}, {29, 31}};

            ASSERT_EQ(clearRanges.size(), expectedRanges.size());
            for (size_t i = 0; i < clearRanges.size(); ++i) {
                ASSERT_EQ(clearRanges[i].beginIndex,
                          expectedRanges[i].beginIndex);
                ASSERT_EQ(clearRanges[i].endIndex, expectedRanges[i].endIndex);
            }

            segment.GetBitmap().Set();
            clearRanges.clear();
            setRanges.clear();
            segment.ClearDiscard(1, segmentSize - 2);

            segment.GetBitmap().Divide(0, endIndex, &clearRanges, &setRanges);
            ASSERT_EQ(1, clearRanges.size());
            ASSERT_EQ(0, clearRanges.front().beginIndex);
            ASSERT_EQ(endIndex, clearRanges.front().endIndex);
        }
    }
}

}  // namespace client
}  // namespace curve
