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

/*
 * Project: curve
 * Date: Thursday Dec 09 21:18:37 CST 2021
 * Author: wuhanqing
 */

#include "src/client/client_common.h"

#include <glog/logging.h>

#include <mutex>
#include <sstream>

#include "src/common/macros.h"

#define APPEND_FLAGS(openflags, flags) \
    do {                               \
        if (openflags & flags) {       \
            openflags &= ~flags;       \
            if (sep) {                 \
                oss << "|";            \
            }                          \
            oss << STRINGIFY(flags);   \
            sep = true;                \
        }                              \
    } while (false)

namespace curve {
namespace client {

std::string OpenflagsToString(int openflags) {
    if (openflags == 0) {
        return "Invalid";
    }

    std::ostringstream oss;
    bool sep = false;

    APPEND_FLAGS(openflags, CURVE_EXCLUSIVE);
    APPEND_FLAGS(openflags, CURVE_SHARED);
    APPEND_FLAGS(openflags, CURVE_RDWR);
    APPEND_FLAGS(openflags, CURVE_RDONLY);
    APPEND_FLAGS(openflags, CURVE_WRONLY);
    APPEND_FLAGS(openflags, CURVE_FORCE_WRITE);

    assert(openflags == 0);

    return oss.str();
}

bool CheckOpenflags(int openflags) {
    if (openflags == 0) {
        return false;
    }

    if ((openflags & CURVE_EXCLUSIVE) && (openflags & CURVE_SHARED)) {
        LOG(WARNING)
            << "Open with `CURVE_EXCLUSIVE` and `CURVE_SHARED` is invalid";
        return false;
    }

    return true;
}

OpenFlags DefaultReadonlyOpenFlags() {
    static OpenFlags readonlyFlags;
    static std::once_flag onceFlag;

    std::call_once(onceFlag, []() { readonlyFlags.exclusive = false; });

    return readonlyFlags;
}

}  // namespace client
}  // namespace curve
