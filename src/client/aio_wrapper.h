/*
 *  Copyright (c) 2023 NetEase Inc.
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

#ifndef CLIENT_AIO_WRAPPER_H_
#define CLIENT_AIO_WRAPPER_H_

#include <bthread/condition_variable.h>
#include <bthread/mutex.h>

#include <cassert>
#include <memory>

#include "include/client/libcurve_define.h"

namespace curve {
namespace client {

class AioWrapper {
 public:
    static std::unique_ptr<AioWrapper> Read(off_t offset,
                                            size_t len,
                                            char* buf) {
        std::unique_ptr<AioWrapper> wrapper{
            new AioWrapper{LIBCURVE_OP::LIBCURVE_OP_READ, offset, len, buf}};
        return wrapper;
    }

    static std::unique_ptr<AioWrapper> Write(off_t offset,
                                            size_t len,
                                            void* buf) {
        std::unique_ptr<AioWrapper> wrapper{
            new AioWrapper{LIBCURVE_OP::LIBCURVE_OP_WRITE, offset, len, buf}};
        return wrapper;
    }

    static std::unique_ptr<AioWrapper> Discard(off_t offset,
                                               size_t len) {
        std::unique_ptr<AioWrapper> wrapper{new AioWrapper{
            LIBCURVE_OP::LIBCURVE_OP_DISCARD, offset, len, nullptr}};
        return wrapper;
    }

    int Wait() const {
        std::unique_lock<bthread::Mutex> lock{mutex};
        while (!done) {
            cond.wait(lock);
        }

        return ctx.ret;
    }

    void Complete() {
        std::unique_lock<bthread::Mutex> lock{mutex};
        done = true;
        cond.notify_all();
    }

    CurveAioContext* Context() { return &ctx; }

    static void Callback(CurveAioContext* context);

 private:
    AioWrapper(LIBCURVE_OP op, off_t offset, size_t length, void* buffer) {
        ctx.op = op;
        ctx.buf = buffer;
        ctx.offset = offset;
        ctx.length = length;
        ctx.ret = -LIBCURVE_ERROR::FAILED;
        ctx.cb = Callback;
    }

    AioWrapper(const AioWrapper&) = delete;
    AioWrapper& operator=(const AioWrapper&) = delete;

    CurveAioContext ctx;
    bool done{false};

    mutable bthread::Mutex mutex;
    mutable bthread::ConditionVariable cond;
};

}  // namespace client
}  // namespace curve

#endif  // CLIENT_AIO_WRAPPER_H_
