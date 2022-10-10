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
#ifndef NAPI_SCREENLOCK_ABILITY_H
#define NAPI_SCREENLOCK_ABILITY_H

#include "async_call.h"
#include "napi/native_common.h"
#include "napi/native_node_api.h"
#include "screenlock_system_ability_interface.h"

namespace OHOS {
namespace ScreenLock {
struct AsyncScreenLockInfo : public AsyncCall::Context {
    napi_status status;
    bool allowed;
    AsyncScreenLockInfo() : Context(nullptr, nullptr), allowed(false) {};
    AsyncScreenLockInfo(InputAction input, OutputAction output)
        : Context(std::move(input), std::move(output)), allowed(false) {};
    virtual ~AsyncScreenLockInfo() override {};
    napi_status operator()(const napi_env env, size_t argc, napi_value argv[], napi_value self) override
    {
        NAPI_ASSERT_BASE(env, self != nullptr, "self is nullptr", napi_invalid_arg);
        return Context::operator()(env, argc, argv, self);
    }
    napi_status operator()(const napi_env env, napi_value *result) override
    {
        if (status != napi_ok) {
            return status;
        }
        return Context::operator()(env, result);
    }
};

struct SendEventInfo : public AsyncCall::Context {
    int32_t param;
    std::string eventInfo;
    bool flag;
    napi_status status;
    bool allowed;
    SendEventInfo() : Context(nullptr, nullptr), flag(false), status(napi_generic_failure), allowed(false) {};
    SendEventInfo(InputAction input, OutputAction output)
        : Context(std::move(input), std::move(output)), flag(false), status(napi_generic_failure), allowed(false) {};
    virtual ~SendEventInfo() override {};
    napi_status operator()(const napi_env env, size_t argc, napi_value argv[], napi_value self) override
    {
        NAPI_ASSERT_BASE(env, self != nullptr, "self is nullptr", napi_invalid_arg);
        return Context::operator()(env, argc, argv, self);
    }

    napi_status operator()(const napi_env env, napi_value *result) override
    {
        if (status != napi_ok) {
            return status;
        }
        return Context::operator()(env, result);
    }
};

struct ScreenlockOnCallBack {
    napi_env env;
    napi_ref callbackref;
    napi_value thisVar;
    SystemEvent systemEvent;
    ErrorInfo errorInfo;
    napi_deferred deferred = nullptr;
    bool callBackResult = false;
};

napi_status IsVaildEvent(const std::string &type);
napi_status CheckParamNumber(size_t argc, std::uint32_t paramNumber);
napi_status CheckParamType(napi_env env, napi_value jsType, napi_status status);
void ThrowError(napi_env env, const uint32_t &code, const std::string &msg);
void GetErrorInfo(int32_t errorCode, ErrorInfo &errorInfo);
std::string GetErrorMessage(const uint32_t &code);
napi_status Init(napi_env env, napi_value exports);
napi_value NAPI_IsScreenLocked(napi_env env, napi_callback_info info);
napi_value NAPI_IsLocked(napi_env env, napi_callback_info info);
napi_value NAPI_UnlockScreen(napi_env env, napi_callback_info info);
napi_value NAPI_Unlock(napi_env env, napi_callback_info info);
napi_value NAPI_Lock(napi_env env, napi_callback_info info);
napi_value NAPI_IsSecureMode(napi_env env, napi_callback_info info);
napi_value NAPI_IsSecure(napi_env env, napi_callback_info info);
napi_value NAPI_ScreenLockSendEvent(napi_env env, napi_callback_info info);
napi_value NAPI_OnSystemEvent(napi_env env, napi_callback_info info);
napi_value NAPI_TestSetScreenLocked(napi_env env, napi_callback_info info);
napi_value NAPI_TestRuntimeNotify(napi_env env, napi_callback_info info);
napi_value NAPI_TestGetRuntimeState(napi_env env, napi_callback_info info);
} // namespace ScreenLock
} // namespace OHOS
#endif //  NAPI_SCREENLOCK_ABILITY_H