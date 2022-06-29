/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef SCREENLOCK_GET_INFO_CALLBACK_H
#define SCREENLOCK_GET_INFO_CALLBACK_H

#include "useridm_callback.h"

namespace OHOS {
namespace ScreenLock {
class ScreenLockGetInfoCallback final : public OHOS::UserIam::UserIDM::GetInfoCallback {
public:
    /**
     * @brief
     * @param info.
     * @return void.
     */
    void OnGetInfo(std::vector<OHOS::UserIam::UserIDM::CredentialInfo> &info) override;
};
} // namespace ScreenLock
} // namespace OHOS
#endif // SCREENLOCK_GET_INFO_CALLBACK_H