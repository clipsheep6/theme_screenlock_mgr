/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SERVICES_INCLUDE_SCLOCK_SERVICE_STUB_H
#define SERVICES_INCLUDE_SCLOCK_SERVICE_STUB_H

#include <cstdint>

#include "iremote_stub.h"
#include "screenlock_manager_interface.h"

namespace OHOS {
namespace ScreenLock {
class ScreenLockManagerStub : public IRemoteStub<ScreenLockManagerInterface> {
public:
    int32_t OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;

private:
    int32_t OnIsLocked(Parcel &data, Parcel &reply);
    int32_t OnIsScreenLocked(Parcel &data, Parcel &reply);
    int32_t OnGetSecure(Parcel &data, Parcel &reply);
    int32_t OnUnlock(MessageParcel &data, MessageParcel &reply);
    int32_t OnUnlockScreen(MessageParcel &data, MessageParcel &reply);
    int32_t OnLock(MessageParcel &data, MessageParcel &reply);
    int32_t OnSendScreenLockEvent(MessageParcel &data, MessageParcel &reply);
    int32_t OnScreenLockOn(MessageParcel &data, MessageParcel &reply);
    int32_t OnLockScreen(MessageParcel &data, MessageParcel &reply);
    int32_t OnIsScreenLockDisabled(MessageParcel &data, MessageParcel &reply);
    int32_t OnSetScreenLockDisabled(MessageParcel &data, MessageParcel &reply);
};
} // namespace ScreenLock
} // namespace OHOS
#endif // SERVICES_INCLUDE_SCLOCK_SERVICE_STUB_H