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
 * Date: Wednesday Apr 20 14:13:56 CST 2022
 * Author: wuhanqing
 */

#include "curvefs/src/client/volume/extent_slice.h"

#include <algorithm>

#include "absl/types/optional.h"
#include "curvefs/proto/metaserver.pb.h"
#include "curvefs/src/client/volume/extent.h"
#include "curvefs/src/client/volume/extent_cache.h"
#include "src/common/fast_align.h"

namespace curvefs {
namespace client {

using ::curve::common::align_down;
using ::curve::common::align_up;
using ::curve::common::is_aligned;

ExtentSlice::ExtentSlice(uint64_t offset) : offset_(offset) {}

ExtentSlice::ExtentSlice(const VolumeExtentSlice& slice)
    : offset_(slice.offset()) {
    for (const auto& pext : slice.extents()) {
        extents_.emplace(
            pext.fsoffset(),
            PExtent{pext.length(), pext.volumeoffset(), !pext.isused()});
    }
}

// ExtentSlice ExtentSlice::Create(const VolumeExtentSlice& slice) {
//     ExtentSlice extentSlice;
//     extentSlice.offset_ = slice.offset();

//     for (const auto& pext : slice.extents()) {
//         extentSlice.extents_.emplace(
//             pext.fsoffset(),
//             PExtent{pext.length(), pext.volumeoffset(), !pext.isused()});
//     }

//     return extentSlice;
// }

VolumeExtentSlice ExtentSlice::ToVolumeExtentSlice() const {
    VolumeExtentSlice slice;

    slice.set_offset(offset_);

    for (const auto& ext : extents_) {
        auto* pext = slice.add_extents();
        pext->set_fsoffset(ext.first);
        pext->set_volumeoffset(ext.second.pOffset);
        pext->set_length(ext.second.len);
        pext->set_isused(!ext.second.UnWritten);
    }

    return slice;
}

void ExtentSlice::DivideForWrite(uint64_t offset,
                                 uint64_t len,
                                 const char* data,
                                 std::vector<WritePart>* allocated,
                                 std::vector<AllocPart>* needAlloc) const {
    uint64_t curOff = offset;
    const char* datap = data;
    const uint64_t curEnd = offset + len;

    auto lower = extents_.lower_bound(curOff);
    const auto upper = extents_.upper_bound(curEnd);

    absl::optional<uint64_t> leftHintOffset;
    absl::optional<uint64_t> rightHintOffset;

    auto setAllocHint = [&leftHintOffset, &rightHintOffset](AllocPart* part) {
        if (leftHintOffset) {
            part->allocInfo.leftHintAvailable = true;
            part->allocInfo.pOffsetLeft = leftHintOffset.value();
        } else if (rightHintOffset) {
            part->allocInfo.rightHintAvailable = true;
            part->allocInfo.pOffsetRight = rightHintOffset.value();
        }
    };

    if (lower != extents_.begin()) {
        --lower;
    }

    while (curOff < curEnd && lower != upper) {
        const auto extStart = lower->first;
        const auto extEnd = lower->first + lower->second.len;

        if (curOff < extStart) {
            AllocPart part;
            part.data = datap;
            if (curEnd <= extStart) {
                // write    |----|           |----|
                // extent        |----|               |----|
                auto alignedoffset =
                    align_down(curOff, ExtentCache::option_.blocksize);
                auto alignedend =
                    align_up(curEnd, ExtentCache::option_.blocksize);

                assert(alignedend <= extStart);

                part.allocInfo.lOffset = alignedoffset;
                part.allocInfo.len = alignedend - alignedoffset;
                part.writelength = curEnd - curOff;
                part.padding = curOff - alignedoffset;

                // only set one side hint
                if (curEnd == extStart && !leftHintOffset) {
                    rightHintOffset = lower->second.pOffset;
                }

                ++lower;
            } else {
                // write   |-----|       |-------|    |--------|
                // extent      |----|       |----|      |----|
                auto alignedoffset =
                    align_down(curOff, ExtentCache::option_.blocksize);

                assert(is_aligned(extStart, ExtentCache::option_.blocksize));

                part.allocInfo.lOffset = alignedoffset;
                part.allocInfo.len = extStart - alignedoffset;
                part.writelength = extStart - curOff;
                part.padding = curOff - alignedoffset;

                // only set one side hint
                if (!leftHintOffset) {
                    rightHintOffset = lower->second.pOffset;
                }
            }

            curOff += part.writelength;
            datap += part.writelength;
            setAllocHint(&part);
            needAlloc->push_back(part);
        } else if (curOff == extStart) {
            WritePart part;
            part.data = datap;

            // write   |----|   |----|      |-------|
            // extent  |----|   |--------|  |----|
            if (curEnd <= extEnd) {
                // write   |----|   |----|
                // extent  |----|   |--------|
                part.offset = lower->second.pOffset;
                part.length = (curEnd - curOff);
            } else {
                // write   |-------|
                // extent  |----|
                part.offset = lower->second.pOffset;
                part.length = (extEnd - extStart);
                leftHintOffset = lower->second.pOffset + lower->second.len;
                ++lower;
            }

            curOff += part.length;
            datap += part.length;
            allocated->push_back(part);
        } else {  // curOff > extStart
            WritePart part;
            part.data = datap;
            // write           |----|        |----|      |----|   |-----|        |----|   // NOLINT(whitespace/line_length)
            // extents  |----|          |----|         |------|  |-------|   |-----|      // NOLINT(whitespace/line_length)
            if (curOff >= extEnd) {
                // write           |----|        |----|
                // extents  |----|          |----|
                if (curOff == extEnd ||
                    align_down(curOff, ExtentCache::option_.blocksize) ==
                        extEnd) {
                    leftHintOffset = lower->second.pOffset + lower->second.len;
                }

                ++lower;
                continue;
            } else if (curEnd <= extEnd) {
                // write       |----|    |----|
                // extents   |------|  |--------|
                part.offset = lower->second.pOffset + (curOff - extStart);
                part.length = curEnd - curOff;
            } else {  // curend > extExt
                // write        |----|
                // extents    |----|
                part.offset = lower->second.pOffset + (curOff - extStart);
                part.length = extEnd - curOff;
                leftHintOffset = lower->second.pOffset + lower->second.len;
                ++lower;
            }

            curOff += part.length;
            datap += part.length;
            allocated->push_back(part);
        }
    }

    if (curOff < curEnd) {
        AllocPart part;
        part.data = datap;

        auto alignedoffset = align_down(curOff, ExtentCache::option_.blocksize);
        auto alignedend = align_up(curEnd, ExtentCache::option_.blocksize);

        part.allocInfo.lOffset = alignedoffset;
        part.allocInfo.len =
            align_up(std::max(alignedend - alignedoffset,
                              ExtentCache::option_.preallocSize),
                     ExtentCache::option_.blocksize);

        if (upper != extents_.end()) {
            part.allocInfo.len =
                std::min(upper->first - alignedoffset, part.allocInfo.len);
            assert(
                is_aligned(part.allocInfo.len, ExtentCache::option_.blocksize));
        }

        part.writelength = curEnd - curOff;
        part.padding = curOff - alignedoffset;
        setAllocHint(&part);
        needAlloc->push_back(part);
    }
}

void ExtentSlice::DivideForRead(uint64_t offset,
                                uint64_t len,
                                char* data,
                                std::vector<ReadPart>* reads,
                                std::vector<ReadPart>* holes) const {
    uint64_t curOff = offset;
    const uint64_t curEnd = offset + len;
    char* datap = data;

    auto lower = extents_.lower_bound(curOff);
    const auto upper = extents_.upper_bound(curEnd);

    if (lower != extents_.begin()) {
        --lower;
    }

    while (curOff < curEnd && lower != upper) {
        const auto extStart = lower->first;
        const auto extEnd = lower->first + lower->second.len;

        if (curOff < extStart) {
            if (curEnd <= extStart) {
                // read    |----|           |----|
                // extent       |----|               |----|
                holes->emplace_back(curOff, curEnd - curOff, datap);
                return;
            } else {
                // read    |----|        |-------|    |--------|
                // extent     |----|        |----|      |----|
                holes->emplace_back(curOff, extStart - curOff, datap);
            }

            datap += (extStart - curOff);
            curOff = extStart;
            // ++lower;
        } else if (curOff == extStart) {
            if (curEnd <= extEnd) {
                // read    |----|   |----|
                // extent  |----|   |--------|
                if (lower->second.UnWritten) {
                    holes->emplace_back(curOff, curEnd - curOff, datap);
                    return;
                } else {
                    reads->emplace_back(lower->second.pOffset, curEnd - curOff,
                                        datap);
                    return;
                }
            } else {
                // read    |-------|
                // extent  |----|
                if (lower->second.UnWritten) {
                    holes->emplace_back(curOff, extEnd - curOff, datap);
                } else {
                    reads->emplace_back(lower->second.pOffset, extEnd - curOff,
                                        datap);
                }

                datap += (extEnd - curOff);
                curOff = extEnd;
                ++lower;
            }
        } else {
            if (curOff >= extEnd) {
                // read            |----|        |----|
                // extents  |----|          |----|
                // do nothing, try next one
                ++lower;
                continue;
            } else if (curEnd <= extEnd) {
                // read        |----|    |----|
                // extents   |------|  |--------|
                if (lower->second.UnWritten) {
                    holes->emplace_back(curOff, curEnd - curOff, datap);
                } else {
                    reads->emplace_back(
                        lower->second.pOffset + (curOff - extStart),
                        curEnd - curOff, datap);
                }

                return;
            } else {
                // read         |----|
                // extents    |----|
                if (lower->second.UnWritten) {
                    holes->emplace_back(curOff, extEnd - curOff, datap);
                } else {
                    reads->emplace_back(
                        lower->second.pOffset + (curOff - extStart),
                        extEnd - curOff, datap);
                }

                datap += (extEnd - curOff);
                curOff = extEnd;
                ++lower;
            }
        }
    }

    if (curOff < curEnd) {
        holes->emplace_back(curOff, curEnd - curOff, datap);
    }
}

void ExtentSlice::Merge(uint64_t loffset, const PExtent &extent) {
    if (extents_.empty()) {
        extents_.emplace(loffset, extent);
        return;
    }

    // try merge with leftside
    auto it = extents_.lower_bound(loffset);
    auto inserted = extents_.end();

    if (it != extents_.begin()) {
        --it;
    }

    if (!it->second.UnWritten && it->first + it->second.len == loffset &&
        it->second.pOffset + it->second.len == extent.pOffset) {
        it->second.len += extent.len;
        inserted = it;
    } else {
        auto r = extents_.emplace(loffset, extent);
        inserted = r.first;
    }

    // try merge with rightside
    const auto endOff = inserted->first + inserted->second.len;
    it = extents_.lower_bound(endOff);

    if (!it->second.UnWritten && it != extents_.end() && it->first == endOff &&
        inserted->second.pOffset + inserted->second.len == it->second.pOffset) {
        inserted->second.len += it->second.len;
        extents_.erase(it);
    }
}

void ExtentSlice::MarkWritten(uint64_t offset, uint64_t len) {
    uint64_t curOff = offset;
    const uint64_t curEnd = offset + len;

    auto lower = extents_.lower_bound(curOff);
    const auto upper = extents_.upper_bound(curEnd);
    auto prev = extents_.end();

    if (lower != extents_.begin()) {
        --lower;
    }

    auto nonoverlap = [](uint64_t off1, uint64_t len1, uint64_t off2,
                         uint64_t len2) {
        return off1 + len1 <= off2 || off2 + len2 <= off1;
    };

    // merge written extents
    auto mergeable =
        [this, &prev](
            std::map<uint64_t, curvefs::client::PExtent>::iterator current) {
            return prev != extents_.end() &&
                   !prev->second.UnWritten &&
                   !current->second.UnWritten &&
                   // logical is continuous
                   prev->first + prev->second.len == current->first &&
                   // physical is continuous
                   prev->second.pOffset + prev->second.len ==
                       current->second.pOffset;
        };

    while (curOff < curEnd && lower != upper) {
        if (nonoverlap(curOff, curEnd - curOff, lower->first,
                       lower->second.len)) {
            prev = lower;
            ++lower;
            continue;
        }

        if (!lower->second.UnWritten) {
            if (mergeable(lower)) {
                prev->second.len += lower->second.len;
                lower = extents_.erase(lower);
            } else {
                prev = lower;
                ++lower;
            }

            continue;
        }

        const auto extStart = lower->first;
        const auto extEnd = lower->first + lower->second.len;

        if (curOff < extStart) {
            if (curEnd >= extEnd) {
                // write     |-------|    |--------|
                // extent       |----|      |----|
                lower->second.UnWritten = false;
                if (mergeable(lower)) {
                    prev->second.len += lower->second.len;
                    lower = extents_.erase(lower);
                } else {
                    prev = lower;
                    ++lower;
                }

                curOff = extEnd;
            } else {
                // write   |----|
                // extent      |----|
                uint64_t overlap = curEnd - extStart;
                PExtent sep;
                sep.pOffset = lower->second.pOffset + overlap;
                sep.len = lower->second.len - overlap;
                sep.UnWritten = true;

                lower->second.len = overlap;
                lower->second.UnWritten = false;

                extents_.emplace(extStart + overlap, sep);
                return;
            }
        } else if (curOff == extStart) {
            // write   |----|   |----|      |-------|
            // extent  |----|   |--------|  |----|
            if (extEnd <= curEnd) {
                // write   |----|   |-------|
                // extent  |----|   |----|
                lower->second.UnWritten = false;
                if (mergeable(lower)) {
                    prev->second.len += lower->second.len;
                    lower = extents_.erase(lower);
                } else {
                    prev = lower;
                    ++lower;
                }

                curOff = extEnd;
            } else {
                // write   |----|
                // extent  |--------|
                uint64_t overlap = curEnd - curOff;
                PExtent sep;
                sep.pOffset = lower->second.pOffset + overlap;
                sep.len = lower->second.len - overlap;
                sep.UnWritten = true;

                lower->second.len = overlap;
                lower->second.UnWritten = false;

                if (mergeable(lower)) {
                    prev->second.len += lower->second.len;
                    extents_.erase(lower);
                }

                extents_.emplace(curEnd, sep);
                return;
            }
        } else {
            // write       |----|    |----|       |----|
            // extents   |------|  |--------|   |----|
            if (curEnd == extEnd) {
                // write       |----|
                // extents   |------|
                uint64_t overlap = curEnd - curOff;
                PExtent sep;
                sep.pOffset = lower->second.pOffset + (curOff - extStart);
                sep.len = overlap;
                sep.UnWritten = false;

                lower->second.len -= overlap;

                extents_.emplace(curOff, sep);

                // 这里是不是可以直接跳出了
                return;
            } else if (curEnd < extEnd) {
                // write        |----|
                // extents    |--------|
                uint64_t overlap = curEnd - curOff;

                PExtent middle;
                middle.pOffset = lower->second.pOffset + (curOff - extStart);
                middle.len = overlap;
                middle.UnWritten = false;

                PExtent right;
                right.pOffset = middle.pOffset + middle.len;
                right.len = extEnd - curEnd;
                right.UnWritten = true;

                lower->second.len = curOff - extStart;

                extents_.emplace(curOff, middle);
                extents_.emplace(curEnd, right);

                return;
            } else {
                // write        |----|
                // extents    |----|

                uint64_t overlap = extEnd - curOff;
                PExtent sep;
                sep.pOffset = lower->second.pOffset + (curOff - extStart);
                sep.len = overlap;
                sep.UnWritten = false;

                lower->second.len -= overlap;

                extents_.emplace(curOff, sep);

                curOff = extEnd;
                prev = lower;
                ++lower;
            }
        }
    }
}

}  // namespace client
}  // namespace curvefs
