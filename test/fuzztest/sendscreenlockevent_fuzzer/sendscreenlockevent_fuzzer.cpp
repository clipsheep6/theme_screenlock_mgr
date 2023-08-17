/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * miscservices under the License is miscservices on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "sendscreenlockevent_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <string_ex.h>

#include "message_parcel.h"
#include "screenlock_system_ability.h"
#include "screenlock_server_ipc_interface_code.h"

using namespace OHOS::ScreenLock;

namespace OHOS {
constexpr int32_t THRESHOLD = 4;
const std::u16string SCREENLOCK_MANAGER_INTERFACE_TOKEN = u"ohos.screenlock.ScreenLockManagerInterface";

bool FuzzSendScreenlockEventTest(const uint8_t *rawData, size_t size)
{
    uint32_t code = static_cast<uint32_t>(ScreenLockServerIpcInterfaceCode::SEND_SCREENLOCK_EVENT);
    MessageParcel data;
    data.WriteInterfaceToken(SCREENLOCK_MANAGER_INTERFACE_TOKEN);
    data.WriteBuffer(rawData, size);
    data.RewindRead(0);
    MessageParcel reply;
    MessageOption option;
    ScreenLockSystemAbility::GetInstance()->OnRemoteRequest(code, data, reply, option);
    return true;
}

} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < OHOS::THRESHOLD) {
        return 0;
    }

    /* Run your code on data */
    OHOS::FuzzSendScreenlockEventTest(data, size);
    ScreenLockSystemAbility::GetInstance()->ResetFfrtQueue();
    return 0;
}