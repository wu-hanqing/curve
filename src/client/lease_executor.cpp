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
 * File Created: Saturday, 23rd February 2019 1:41:31 pm
 * Author: tongguangxun
 */
#include <glog/logging.h>

#include "src/common/timeutility.h"
#include "src/client/lease_executor.h"
#include "src/client/service_helper.h"

using curve::common::TimeUtility;

namespace curve {
namespace client {

LeaseExecutor::LeaseExecutor(const LeaseOption& leaseOpt,
                             MDSClient* mdsclient,
                             IOManager4File* iomanager)
    : mdsclient_(mdsclient),
      iomanager_(iomanager),
      leaseoption_(leaseOpt),
      leasesession_(),
      isleaseAvaliable_(true),
      failedrefreshcount_(0),
      task_(),
      fileInfo_(nullptr) {}

LeaseExecutor::~LeaseExecutor() {
    if (task_) {
        task_->Stop();
        task_->WaitTaskExit();
    }
}

bool LeaseExecutor::Start(const FInfo& fi, const LeaseSession& lease) {
    fileInfo_ = &fi;
    // fullFileName_ = fi.fullPathName;
    // openflags_ = fi.openflags;
    // openId_ = fi.openId;
    leasesession_ = lease;

    if (leasesession_.leaseTime <= 0) {
        LOG(ERROR) << "Invalid lease time, filename = " << fi.fullPathName;
        return false;
    }

    if (leaseoption_.mdsRefreshTimesPerLease == 0) {
        LOG(ERROR) << "Invalid refreshTimesPerLease, filename = "
                   << fi.fullPathName;
        return false;
    }

    iomanager_->UpdateFileInfo(fi);

    auto interval =
        leasesession_.leaseTime / leaseoption_.mdsRefreshTimesPerLease;

    task_.reset(new (std::nothrow) RefreshSessionTask(this, interval));
    if (task_ == nullptr) {
        LOG(ERROR) << "Allocate RefreshSessionTask failed, filename = "
                   << fi.fullPathName;
        return false;
    }

    timespec abstime = butil::microseconds_from_now(interval);
    brpc::PeriodicTaskManager::StartTaskAt(task_.get(), abstime);

    LOG(INFO) << "LeaseExecutor for " << fi.fullPathName
              << " started, lease interval is " << interval << " us";

    return true;
}

bool LeaseExecutor::RefreshLease() {
    if (!LeaseValid()) {
        LOG(INFO) << "lease not valid!";
        iomanager_->LeaseTimeoutBlockIO();
    }

    LeaseRefreshResult response;
    LIBCURVE_ERROR ret = mdsclient_->RefreshSession(fileInfo_, &response);

    if (LIBCURVE_ERROR::FAILED == ret) {
        LOG(WARNING) << "Refresh session rpc failed, filename = "
                     << fileInfo_->fullPathName;
        return true;
    } else if (LIBCURVE_ERROR::AUTHFAIL == ret) {
        iomanager_->LeaseTimeoutBlockIO();
        LOG(ERROR) << "Refresh session auth fail, block io. "
                   << "session id = " << leasesession_.sessionID
                   << ", filename = " << fileInfo_->fullPathName;
        return true;
    }

    if (response.status == LeaseRefreshResult::Status::OK) {
        if (iomanager_->InodeId() != response.finfo.id) {
            LOG(ERROR) << fileInfo_->fullPathName
                       << " inode id changed, current id = "
                       << iomanager_->InodeId()
                       << ", but mds response id = " << response.finfo.id
                       << ", block IO";
            iomanager_->LeaseTimeoutBlockIO();
            isleaseAvaliable_.store(false);
            return false;
        }

        CheckNeedUpdateFileInfo(response.finfo);
        failedrefreshcount_.store(0);
        isleaseAvaliable_.store(true);
        iomanager_->ResumeIO();
        return true;
    } else if (response.status == LeaseRefreshResult::Status::NOT_EXIST) {
        iomanager_->LeaseTimeoutBlockIO();
        isleaseAvaliable_.store(false);
        LOG(ERROR) << "session or file not exists, no longer refresh!"
                   << ", sessionid = " << leasesession_.sessionID
                   << ", filename = " << fileInfo_->fullPathName;
        return false;
    } else {
        LOG(ERROR) << "Refresh session failed, filename = "
                   << fileInfo_->fullPathName;
        return true;
    }
    return true;
}

void LeaseExecutor::Stop() {
    if (task_ != nullptr) {
        task_->Stop();

        LOG(INFO) << "LeaseExecutor for " << fileInfo_->filename << " stopped";
    }
}

bool LeaseExecutor::LeaseValid() {
    return isleaseAvaliable_.load();
}

void LeaseExecutor::IncremRefreshFailed() {
    failedrefreshcount_.fetch_add(1);
    if (failedrefreshcount_.load() >= leaseoption_.mdsRefreshTimesPerLease) {
        isleaseAvaliable_.store(false);
        iomanager_->LeaseTimeoutBlockIO();
        LOG(ERROR) << "session invalid now!";
    }
}

void LeaseExecutor::CheckNeedUpdateFileInfo(const FInfo& fileInfo) {
    MetaCache* metaCache = iomanager_->GetMetaCache();

    uint64_t currentFileSn = metaCache->GetLatestFileSn();
    uint64_t newSn = fileInfo.seqnum;
    if (newSn > currentFileSn) {
        LOG(INFO) << "Update file sn, new file sn = " << newSn
                  << ", current sn = " << currentFileSn
                  << ", filename = " << fileInfo.filename;
        metaCache->SetLatestFileSn(newSn);
    }

    FileStatus currentFileStatus = metaCache->GetLatestFileStatus();
    FileStatus newFileStatus = fileInfo.filestatus;
    if (newFileStatus != currentFileStatus) {
        LOG(INFO) << "Update file status, new status = "
                  << FileStatusToName(newFileStatus)
                  << ", current file status = "
                  << FileStatusToName(currentFileStatus)
                  << ", filename = " << fileInfo.filename;
        metaCache->SetLatestFileStatus(newFileStatus);
    }
}

void LeaseExecutor::ResetRefreshSessionTask() {
    if (task_ == nullptr) {
        return;
    }

    // 等待前一个任务退出
    task_->Stop();
    task_->WaitTaskExit();

    auto interval = task_->RefreshIntervalUs();

    task_.reset(new (std::nothrow) RefreshSessionTask(this, interval));
    timespec abstime = butil::microseconds_from_now(interval);
    brpc::PeriodicTaskManager::StartTaskAt(task_.get(), abstime);

    isleaseAvaliable_.store(true);
}

}   // namespace client
}   // namespace curve
