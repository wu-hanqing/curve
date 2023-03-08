/*
 *     Copyright (c) 2020 NetEase Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * Project : curve
 * Date : Wed Apr 22 17:04:51 CST 2020
 * Author: wuhanqing
 */

/*
 * rbd-nbd - RBD in userspace
 *
 * Copyright (C) 2015 - 2016 Kylin Corporation
 *
 * Author: Yunchuan Wen <yunchuan.wen@kylin-cloud.com>
 *         Li Wang <li.wang@kylin-cloud.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
*/

#ifndef NBD_SRC_NBDSERVER_H_
#define NBD_SRC_NBDSERVER_H_

#include <linux/nbd.h>
#include <atomic>
#include <condition_variable>  // NOLINT
#include <deque>
#include <memory>
#include <mutex>  // NOLINT
#include <string>
#include <thread>  // NOLINT

#include "nbd/src/ImageInstance.h"
#include "nbd/src/NBDController.h"
#include "nbd/src/SafeIO.h"
#include "nebd/src/part1/libnebd.h"

namespace curve {
namespace nbd {

class NBDServer;

// NBD IO请求上下文信息
struct IOContext {
    struct nbd_request request;
    struct nbd_reply reply;

    // 请求类型
    int command = 0;

    NBDServer* server = nullptr;
    std::unique_ptr<char[]> data;

    // 后端IO请求上下文信息
    AioContext aio;

    IOContext() {
        memset(&aio, 0, sizeof(aio));
    }
};

// NBDServer负责与nbd内核进行数据通信
class NBDServer {
 public:
    NBDServer(int sock,
              NBDControllerPtr nbdCtrl,
              const NBDConfig* config,
              std::shared_ptr<ImageInstance> imageInstance,
              std::shared_ptr<SafeIO> safeIO = std::make_shared<SafeIO>())
        : started_(false),
          terminated_(false),
          sock_(sock),
          nbdCtrl_(nbdCtrl),
          config_(config),
          image_(imageInstance),
          safeIO_(safeIO) {}

    ~NBDServer();

    /**
     * @brief 启动server
     */
    void Start();

    /**
     * 等待断开连接
     */
    void WaitForDisconnect();

    /**
     * 测试使用，返回server是否终止
     */
    bool IsTerminated() const {
        return terminated_;
    }

    NBDControllerPtr GetController() {
        return nbdCtrl_;
    }

    /**
     * @brief 异步请求结束时执行函数
     * @param ctx 异步请求上下文
     */
    void OnRequestFinish(IOContext* ctx);

 private:
    /**
     * @brief 关闭server
     */
    void Shutdown();

    /**
     * 等下已下发请求全部返回
     */
    void WaitClean();

    /**
     * @brief 读线程执行函数
     */
    void ReaderFunc();

    /**
     * @brief 写线程执行函数
     */
    void WriterFunc();

    /**
     * @brief 异步请求开始时执行函数
     */
    void OnRequestStart();

    /**
     * @brief 等待异步请求返回
     * @return 异步请求context
     */
    IOContext* WaitRequestFinish();

    /**
     * 发起异步请求
     * @param ctx nbd请求上下文
     * @return 请求是否发起成功
     */
    bool StartAioRequest(IOContext* ctx);

    void SetUpAioRequest(IOContext* ctx);

 private:
    // server是否启动
    std::atomic<bool> started_;
    // server是否停止
    std::atomic<bool> terminated_;

    // 与内核通信的socket fd
    int sock_;
    NBDControllerPtr nbdCtrl_;
    const NBDConfig* config_;
    std::shared_ptr<ImageInstance> image_;
    std::shared_ptr<SafeIO> safeIO_;

    // 保护pendingRequestCounts_和finishedRequests_
    std::mutex requestMtx_;
    std::condition_variable requestCond_;

    // 正在执行过程中的请求数量
    uint64_t pendingRequestCounts_ = 0;

    // 已完成请求上下文队列
    std::deque<IOContext*> finishedRequests_;

    // 读线程
    std::thread readerThread_;
    // 写线程
    std::thread writerThread_;

    // 等待断开连接锁/条件变量
    std::mutex disconnectMutex_;
    std::condition_variable disconnectCond_;
};
using NBDServerPtr = std::shared_ptr<NBDServer>;

}  // namespace nbd
}  // namespace curve

#endif  // NBD_SRC_NBDSERVER_H_
