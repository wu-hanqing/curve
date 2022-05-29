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
 * Project: Curve
 * Created Date: 2022-05-26
 * Author: YangFan (fansehep)
 */

#include "src/mds/nameserver2/file_writer_lock.h"

#include <utility>

#include "proto/nameserver2.pb.h"
#include "src/common/timeutility.h"
#include "src/mds/nameserver2/helper/namespace_helper.h"

namespace curve {
namespace mds {

using ::curve::common::NameLockGuard;
using ::curve::common::TimeUtility;

FileWriterLockManager::FileWriterLockManager(
    const FileWriterLockOptions& options,
    std::shared_ptr<KVStorageClient> storage)
    : options_(options), storage_(std::move(storage)) {}

FileWriterLockManager::LockStatus FileWriterLockManager::GetCurrentWriter(
    uint64_t inodeId,
    std::string* owner) {
    std::string key = NameSpaceStorageCodec::EncodeFileWriterKey(inodeId);
    std::string value;
    int ret = storage_->Get(key, &value);

    if (ret == EtcdErrCode::EtcdKeyNotExist) {
        return LockStatus::kNotExist;
    }

    if (ret != EtcdErrCode::EtcdOK) {
        LOG(ERROR) << "Failed to get file writer from storage, inodeid: "
                   << inodeId;
        return LockStatus::kInternalError;
    }

    FileWriterLock lock;
    if (!lock.ParseFromString(value)) {
        LOG(ERROR) << "Failed to parse FileWriterLock from string, inodeid: "
                   << inodeId;
        return LockStatus::kInternalError;
    }

    *owner = std::move(*lock.mutable_owner());

    const uint64_t now = TimeUtility::GetTimeofDayUs();
    if (now > lock.expiredus()) {
        LOG(INFO) << "Previous writer lock is expired, last owner: "
                  << *owner;
        return LockStatus::kExpired;
    }

    return LockStatus::kSuccess;
}

bool FileWriterLockManager::RemoveWriter(uint64_t inodeId) {
    std::string key = NameSpaceStorageCodec::EncodeFileWriterKey(inodeId);
    int ret = storage_->Delete(key);
    if (ret != EtcdErrCode::EtcdOK) {
        LOG(WARNING) << "Failed to delete lock from etcd, error: " << ret;
        return false;
    }

    return true;
}

bool FileWriterLockManager::Lock(uint64_t inodeId, const std::string& owner) {
    LOG(INFO) << "Going to acquire file writer lock, inodeid: " << inodeId
              << ", owner: " << owner;

    NameLockGuard lg(namelock_, std::to_string(inodeId));
    std::string lastOwner;
    auto status = GetCurrentWriter(inodeId, &lastOwner);

    if (status == LockStatus::kInternalError) {
        LOG(ERROR) << "Failed to acquire lock, internal error, inodeid: "
                   << inodeId;
        return false;
    }

    if (status == LockStatus::kNotExist || status == LockStatus::kExpired) {
        bool succ = UpdateWriterLock(inodeId, owner);
        LOG_IF(INFO, succ) << "Succeeded to acquire lock, inodeid: " << inodeId
                           << ", owner: " << owner;
        return succ;
    }

    if (lastOwner == owner) {
        UpdateWriterLock(inodeId, owner);
        return true;
    }

    LOG(WARNING) << "Failed to acquire lock, inodeid: " << inodeId
                 << ", current owner: " << lastOwner;
    return false;
}

bool FileWriterLockManager::Update(uint64_t inodeId, const std::string& owner) {
    // TODO: it's for debugging
    LOG(INFO) << "Updating file writer lock, inodeid: " << inodeId
              << ", owner: " << owner;

    NameLockGuard lg(namelock_, std::to_string(inodeId));
    std::string lastOwner;
    auto status = GetCurrentWriter(inodeId, &lastOwner);

    if (status == LockStatus::kSuccess || status == LockStatus::kExpired) {
        if (owner == lastOwner) {
            return UpdateWriterLock(inodeId, owner);
        }
    }

    LOG(WARNING) << "Failed to update writer lock, lock status: " << status
                     << ", last owner: " << lastOwner;
    return false;
}

bool FileWriterLockManager::UpdateWriterLock(uint64_t inodeId,
                                             const std::string& owner) {
    FileWriterLock lock;
    lock.set_expiredus(TimeUtility::GetTimeofDayUs() + options_.ttlUs);
    lock.set_owner(owner);

    std::string key = NameSpaceStorageCodec::EncodeFileWriterKey(inodeId);
    std::string value;

    bool succ = lock.SerializeToString(&value);
    if (!succ) {
        LOG(WARNING) << "Failed to serialize lock to string, inodeid: "
                     << inodeId << ", owner: " << owner;
        return false;
    }

    int ret = storage_->Put(key, value);
    if (ret != EtcdErrCode::EtcdOK) {
        LOG(WARNING) << "Failed to put lock into etcd, inodeid: " << inodeId
                     << ", owner: " << owner;
        return false;
    }

    return true;
}

bool FileWriterLockManager::Unlock(uint64_t inodeId, const std::string& owner) {
    LOG(INFO) << "Going to release file writer lock, inodeid: " << inodeId
              << ", owner: " << owner;

    NameLockGuard lg(namelock_, std::to_string(inodeId));
    std::string lastOwner;

    auto status = GetCurrentWriter(inodeId, &lastOwner);
    if (status == LockStatus::kInternalError) {
        LOG(WARNING) << "Failed to unlock, internal error, inodeid: " << inodeId
                     << ", owner: " << owner;
        return false;
    }

    if (status == FileWriterLockManager::kNotExist) {
        LOG(WARNING) << "File writer lock not exists, inodeId: " << inodeId
                     << ", owner: " << owner;
        return false;
    }

    if (status == LockStatus::kSuccess || status == LockStatus::kExpired) {
        if (lastOwner != owner) {
            LOG(WARNING) << "File writer lock exists, but owner is not "
                            "identical, inodeid: "
                         << inodeId << ", owner: " << owner
                         << ", last owner: " << lastOwner;
            return false;
        }

        bool succ = RemoveWriter(inodeId);
        LOG_IF(INFO, succ) << "Succeeded to release lock, inodeid: " << inodeId
                           << ", last owner: " << owner;
        return succ;
    }

    return false;
}

}  // namespace mds
}  // namespace curve
