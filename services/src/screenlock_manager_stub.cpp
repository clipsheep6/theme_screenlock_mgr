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

#include "screenlock_manager_stub.h"

#include <string>

#include "parcel.h"
#include "sclock_log.h"
#include "screenlock_common.h"
#include "screenlock_system_ability_interface.h"

namespace OHOS {
namespace ScreenLock {
using namespace OHOS::HiviewDFX;
ScreenLockManagerStub::ScreenLockManagerStub()
{
    memberFuncMap_[static_cast<uint32_t>(IS_SCREEN_LOCKED)] = &ScreenLockManagerStub::OnIsScreenLocked;
    memberFuncMap_[static_cast<uint32_t>(IS_SECURE_MODE)] = &ScreenLockManagerStub::OnGetSecure;
    memberFuncMap_[static_cast<uint32_t>(REQUEST_UNLOCK)] = &ScreenLockManagerStub::OnRequestUnlock;
    memberFuncMap_[static_cast<uint32_t>(REQUEST_LOCK)] = &ScreenLockManagerStub::OnRequestLock;
    memberFuncMap_[static_cast<uint32_t>(SEND_SCREENLOCK_EVENT)] = &ScreenLockManagerStub::OnSendScreenLockEvent;
    memberFuncMap_[static_cast<uint32_t>(ONSYSTEMEVENT)] = &ScreenLockManagerStub::OnScreenLockOn;
}

int32_t ScreenLockManagerStub::OnRemoteRequest(
    uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    SCLOCK_HILOGD("OnRemoteRequest started, code = %{public}d", code);
    auto descriptorToken = data.ReadInterfaceToken();
    if (descriptorToken != GetDescriptor()) {
        SCLOCK_HILOGE("Remote descriptor not the same as local descriptor.");
        return E_SCREENLOCK_TRANSACT_ERROR;
    }
    auto itFunc = memberFuncMap_.find(code);
    if (itFunc != memberFuncMap_.end()) {
        auto memberFunc = itFunc->second;
        if (memberFunc != nullptr) {
            return (this->*memberFunc)(data, reply);
        }
    }
    int ret = IPCObjectStub::OnRemoteRequest(code, data, reply, option);
    SCLOCK_HILOGI("end##ret = %{public}d", ret);
    return ret;
}

int32_t ScreenLockManagerStub::OnIsScreenLocked(MessageParcel &data, MessageParcel &reply)
{
    bool result = IsScreenLocked();
    SCLOCK_HILOGD("isLocked is %{public}d", result);
    if (!reply.WriteBool(result)) {
        SCLOCK_HILOGE("WriteBool failed");
        return ERR_INVALID_VALUE;
    }
    SCLOCK_HILOGI("end.");
    return ERR_OK;
}

int32_t ScreenLockManagerStub::OnGetSecure(MessageParcel &data, MessageParcel &reply)
{
    bool result = GetSecure();
    SCLOCK_HILOGD("isSecure is %{public}d", result);
    if (!reply.WriteBool(result)) {
        SCLOCK_HILOGE("WriteBool failed");
        return ERR_INVALID_VALUE;
    }
    SCLOCK_HILOGI("end.");
    return ERR_OK;
}

int32_t ScreenLockManagerStub::OnRequestUnlock(MessageParcel &data, MessageParcel &reply)
{
    sptr<IRemoteObject> remote = data.ReadRemoteObject();
    SCLOCK_HILOGD("ScreenLockManagerStub::OnRequestUnlock  addr=%{public}p", remote.GetRefPtr());
    if (remote == nullptr) {
        SCLOCK_HILOGE("ScreenLockManagerStub::OnRequestUnlock remote is nullptr");
        return ERR_INVALID_VALUE;
    }
    sptr<ScreenLockSystemAbilityInterface> listener = iface_cast<ScreenLockSystemAbilityInterface>(remote);
    SCLOCK_HILOGD("ScreenLockManagerStub::OnRequestUnlock addr=%{public}p", listener.GetRefPtr());
    if (listener.GetRefPtr() == nullptr) {
        SCLOCK_HILOGE("ScreenLockManagerStub::OnRequestUnlock listener is null");
        return ERR_INVALID_VALUE;
    }
    int32_t status = RequestUnlock(listener);
    if (!reply.WriteInt32(status)) {
        SCLOCK_HILOGE("WriteInt32 failed");
        return ERR_INVALID_VALUE;
    }
    SCLOCK_HILOGI("end.");
    return ERR_OK;
}

int32_t ScreenLockManagerStub::OnRequestLock(MessageParcel &data, MessageParcel &reply)
{
    sptr<IRemoteObject> remote = data.ReadRemoteObject();
    if (remote == nullptr) {
        SCLOCK_HILOGE("ScreenLockManagerStub::OnRequestLock remote is nullptr");
        return ERR_INVALID_VALUE;
    }
    sptr<ScreenLockSystemAbilityInterface> listener = iface_cast<ScreenLockSystemAbilityInterface>(remote);
    if (listener.GetRefPtr() == nullptr) {
        SCLOCK_HILOGE("ScreenLockManagerStub::OnRequestLock listener is null");
        return ERR_INVALID_VALUE;
    }
    int32_t status = RequestLock(listener);
    if (!reply.WriteInt32(status)) {
        SCLOCK_HILOGE("WriteInt32 failed");
        return ERR_INVALID_VALUE;
    }
    SCLOCK_HILOGI("end.");
    return ERR_OK;
}

int32_t ScreenLockManagerStub::OnScreenLockOn(MessageParcel &data, MessageParcel &reply)
{
    sptr<IRemoteObject> remote = data.ReadRemoteObject();
    if (remote == nullptr) {
        SCLOCK_HILOGE("ScreenLockManagerStub::OnScreenLockOn remote is nullptr");
        return ERR_INVALID_VALUE;
    }
    sptr<ScreenLockSystemAbilityInterface> listener = iface_cast<ScreenLockSystemAbilityInterface>(remote);
    if (listener.GetRefPtr() == nullptr) {
        SCLOCK_HILOGE("ScreenLockManagerStub::OnScreenLockOn listener is null");
        return ERR_INVALID_VALUE;
    }
    int32_t ret = OnSystemEvent(listener);
    if (!reply.WriteInt32(ret)) {
        SCLOCK_HILOGE("WriteInt32 failed");
        return ERR_INVALID_VALUE;
    }
    SCLOCK_HILOGI("end.");
    return ERR_OK;
}

int32_t ScreenLockManagerStub::OnSendScreenLockEvent(MessageParcel &data, MessageParcel &reply)
{
    std::string event = data.ReadString();
    int param = data.ReadInt32();
    SCLOCK_HILOGD("event=%{public}s ", event.c_str());
    SCLOCK_HILOGD("param=%{public}d ", param);
    int32_t retCode = SendScreenLockEvent(event, param);
    if (!reply.WriteInt32(retCode)) {
        SCLOCK_HILOGE("WriteInt32 failed");
        return ERR_INVALID_VALUE;
    }
    SCLOCK_HILOGI("end.");
    return ERR_OK;
}
} // namespace ScreenLock
} // namespace OHOS