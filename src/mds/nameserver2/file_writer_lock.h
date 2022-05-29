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
 * Created Date: 2022-05-26
 * Author: YangFan (fansehep)
 */
#ifndef SRC_MDS_NAMESERVER2_FILE_WRITER_LOCK_H_
#define SRC_MDS_NAMESERVER2_FILE_WRITER_LOCK_H_

#include <memory>
#include <string>

#include "src/common/concurrent/name_lock.h"
#include "src/common/encode.h"
#include "src/kvstorageclient/etcd_client.h"

namespace curve {
namespace mds {

using ::curve::kvstorage::KVStorageClient;

struct FileWriterLockOptions {
    // Time to live
    //
    // Default: equals to `mds.file.expiredTimeUs` in mds.conf
    uint64_t ttlUs;
};

class FileWriterLockManager {
    enum LockStatus {
        kSuccess,
        kNotExist,
        kExpired,
        kInternalError,
    };

 public:
    FileWriterLockManager(const FileWriterLockOptions& options,
                          std::shared_ptr<KVStorageClient> storage);

    FileWriterLockManager(const FileWriterLockManager&) = delete;
    FileWriterLockManager& operator=(const FileWriterLockManager) = delete;

    virtual ~FileWriterLockManager() = default;

    // Acquire writer lock
    virtual bool Lock(uint64_t inodeId, const std::string& owner);

    // Release writer lock
    virtual bool Unlock(uint64_t inodeId, const std::string& owner);

    virtual bool Update(uint64_t inodeId, const std::string& owner);

 private:
    bool UpdateWriterLock(uint64_t inodeId, const std::string& owner);

    bool RemoveWriter(uint64_t inodeId);

    LockStatus GetCurrentWriter(uint64_t inodeId, std::string* owner);

 private:
    FileWriterLockOptions options_;
    std::shared_ptr<KVStorageClient> storage_;
    curve::common::NameLock namelock_;
};

}  // namespace mds
}  // namespace curve

#endif  // SRC_MDS_NAMESERVER2_FILE_WRITER_LOCK_H_
