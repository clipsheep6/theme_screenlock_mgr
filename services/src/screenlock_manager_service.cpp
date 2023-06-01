/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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
#include "screenlock_manager_service.h"

#include <cerrno>
#include <ctime>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <string>
#include <sys/time.h>
#include <unistd.h>

#include "common_event_support.h"
#include "ability_manager_client.h"
#include "accesstoken_kit.h"
#include "command.h"
#include "common_event_manager.h"
#include "display_manager.h"
#include "dump_helper.h"
#include "hitrace_meter.h"
#include "ipc_skeleton.h"
#include "iservice_registry.h"
#include "os_account_manager.h"
#include "parameter.h"
#include "sclock_log.h"
#include "screenlock_appinfo.h"
#include "screenlock_common.h"
#include "screenlock_get_info_callback.h"
#include "tokenid_kit.h"
#include "user_idm_client.h"
#include "want.h"
#include "xcollie/watchdog.h"

namespace OHOS {
namespace ScreenLock {
using namespace std;
using namespace OHOS::HiviewDFX;
using namespace OHOS::Rosen;
using namespace OHOS::UserIam::UserAuth;
using namespace OHOS::Security::AccessToken;


const std::int64_t TIME_OUT_MILLISECONDS = 10000L;
const std::int64_t DELAY_TIME = 1000L;
const std::int64_t INTERVAL_ZERO = 0L;
std::shared_ptr<AppExecFwk::EventHandler> ScreenLockManagerService::serviceHandler_;
constexpr int32_t MAX_RETRY_TIMES = 20;

ScreenLockManagerService::ScreenLockManagerService()
{
    SCLOCK_HILOGI("ScreenLockManagerTes");
    InitServiceHandler();
    RegisterDMS();
    stateValue_.Reset();
    state_ = ServiceRunningState::STATE_NOT_START;
}

ScreenLockManagerService::~ScreenLockManagerService()
{
    SCLOCK_HILOGI("~ScreenLockManagerService state_  is %{public}d.", static_cast<int>(state_));
    serviceHandler_ = nullptr;
    DisplayManager::GetInstance().UnregisterDisplayPowerEventListener(displayPowerEventListener_);
}

ScreenLockManagerService& ScreenLockManagerService::GetInstance()
{
    static ScreenLockManagerService ScreenLockManagerService;
    return ScreenLockManagerService;
}

void ScreenLockManagerService::RegisterDMS()
{
    SCLOCK_HILOGI("RegisterDMS");
    int times = 0;
    if (displayPowerEventListener_ == nullptr) {
        displayPowerEventListener_ = new ScreenLockManagerService::ScreenLockDisplayPowerEventListener();
    }
    RegisterDisplayPowerEventListener(times);
    if (flag_) {
        state_ = ServiceRunningState::STATE_RUNNING;
        auto callback = [=]() { OnSystemReady(); };
        serviceHandler_->PostTask(callback, INTERVAL_ZERO);
    }
}

void ScreenLockManagerService::RegisterDisplayPowerEventListener(int32_t times)
{
    SCLOCK_HILOGI("RegisterDisplayPowerEventListener");
    times++;
    flag_ = (DisplayManager::GetInstance().RegisterDisplayPowerEventListener(displayPowerEventListener_) ==
             DMError::DM_OK);
    if (flag_ == false && times <= MAX_RETRY_TIMES) {
        SCLOCK_HILOGE("ScreenLockManagerService RegisterDisplayPowerEventListener failed");
        auto callback = [this, times]() { RegisterDisplayPowerEventListener(times); };
        serviceHandler_->PostTask(callback, DELAY_TIME);
    }
    SCLOCK_HILOGI("ScreenLockManagerService RegisterDisplayPowerEventListener end, flag_:%{public}d, times:%{public}d",
        flag_, times);
}

void ScreenLockManagerService::InitServiceHandler()
{
    SCLOCK_HILOGI("InitServiceHandler started.");
    if (serviceHandler_ != nullptr) {
        SCLOCK_HILOGI("InitServiceHandler already init.");
        return;
    }
    std::shared_ptr<AppExecFwk::EventRunner> runner = AppExecFwk::EventRunner::Create("ScreenLockManager");
    serviceHandler_ = std::make_shared<AppExecFwk::EventHandler>(runner);
    if (HiviewDFX::Watchdog::GetInstance().AddThread("ScreenLockManager", serviceHandler_,
        TIME_OUT_MILLISECONDS) != 0) {
        SCLOCK_HILOGE("HiviewDFX::Watchdog::GetInstance AddThread Fail");
    }
    SCLOCK_HILOGI("InitServiceHandler succeeded.");
}

void ScreenLockManagerService::ScreenLockDisplayPowerEventListener::OnDisplayPowerEvent(DisplayPowerEvent event,
    EventStatus status)
{
    SCLOCK_HILOGI("OnDisplayPowerEvent event=%{public}d", static_cast<int>(event));
    SCLOCK_HILOGI("OnDisplayPowerEvent status= %{public}d", static_cast<int>(status));
    if (status == EventStatus::BEGIN) {
        if (event == DisplayPowerEvent::WAKE_UP) {
            GetInstance().OnBeginWakeUp();
        } else if (event == DisplayPowerEvent::SLEEP) {
            GetInstance().OnBeginSleep(0);
        } else if (event == DisplayPowerEvent::DISPLAY_ON) {
            GetInstance().OnBeginScreenOn();
        } else if (event == DisplayPowerEvent::DISPLAY_OFF) {
            GetInstance().OnBeginScreenOff();
        } else if (event == DisplayPowerEvent::DESKTOP_READY) {
            GetInstance().OnExitAnimation();
        }
    } else if (status == EventStatus::END) {
        if (event == DisplayPowerEvent::WAKE_UP) {
            GetInstance().OnEndWakeUp();
        } else if (event == DisplayPowerEvent::SLEEP) {
            GetInstance().OnEndSleep(0, 0);
        } else if (event == DisplayPowerEvent::DISPLAY_ON) {
            GetInstance().OnEndScreenOn();
        } else if (event == DisplayPowerEvent::DISPLAY_OFF) {
            GetInstance().OnEndScreenOff();
        }
    }
}

void ScreenLockManagerService::OnBeginScreenOff()
{
    SCLOCK_HILOGI("ScreenLockManagerService OnBeginScreenOff started.");
    stateValue_.SetScreenState(static_cast<int32_t>(ScreenState::SCREEN_STATE_BEGIN_OFF));
    SystemEvent systemEvent(BEGIN_SCREEN_OFF);
    SystemEventCallBack(systemEvent);
}

void ScreenLockManagerService::OnEndScreenOff()
{
    SCLOCK_HILOGI("ScreenLockManagerService OnEndScreenOff started.");
    stateValue_.SetScreenState(static_cast<int32_t>(ScreenState::SCREEN_STATE_END_OFF));
    SystemEvent systemEvent(END_SCREEN_OFF);
    SystemEventCallBack(systemEvent);
}

void ScreenLockManagerService::OnBeginScreenOn()
{
    SCLOCK_HILOGI("ScreenLockManagerService OnBeginScreenOn started.");
    stateValue_.SetScreenState(static_cast<int32_t>(ScreenState::SCREEN_STATE_BEGIN_ON));
    SystemEvent systemEvent(BEGIN_SCREEN_ON);
    SystemEventCallBack(systemEvent);
}

void ScreenLockManagerService::OnSystemReady()
{
    SCLOCK_HILOGI("ScreenLockManagerService OnSystemReady started.");
    bool isExitFlag = false;
    int tryTime = 20;
    int minTryTime = 0;
    while (!isExitFlag && (tryTime > minTryTime)) {
        if (systemEventListener_ != nullptr) {
            SCLOCK_HILOGI("ScreenLockManagerService OnSystemReady started1.");
            std::lock_guard<std::mutex> lck(listenerMutex_);
            SystemEvent systemEvent(SYSTEM_READY);
            systemEventListener_->OnCallBack(systemEvent);
            isExitFlag = true;
        } else {
            SCLOCK_HILOGE("ScreenLockManagerService OnSystemReady type not found., flag_ = %{public}d", flag_);
            sleep(1);
        }
        --tryTime;
    }
}

void ScreenLockManagerService::OnEndScreenOn()
{
    SCLOCK_HILOGI("ScreenLockManagerService OnEndScreenOn started.");
    stateValue_.SetScreenState(static_cast<int32_t>(ScreenState::SCREEN_STATE_END_ON));
    SystemEvent systemEvent(END_SCREEN_ON);
    SystemEventCallBack(systemEvent);
}

void ScreenLockManagerService::OnBeginWakeUp()
{
    SCLOCK_HILOGI("ScreenLockManagerService OnBeginWakeUp started.");
    stateValue_.SetInteractiveState(static_cast<int32_t>(InteractiveState::INTERACTIVE_STATE_BEGIN_WAKEUP));
    SystemEvent systemEvent(BEGIN_WAKEUP);
    SystemEventCallBack(systemEvent);
}

void ScreenLockManagerService::OnEndWakeUp()
{
    SCLOCK_HILOGI("ScreenLockManagerService OnEndWakeUp started.");
    stateValue_.SetInteractiveState(static_cast<int32_t>(InteractiveState::INTERACTIVE_STATE_END_WAKEUP));
    SystemEvent systemEvent(END_WAKEUP);
    SystemEventCallBack(systemEvent);
}

void ScreenLockManagerService::OnBeginSleep(const int why)
{
    SCLOCK_HILOGI("ScreenLockManagerService OnBeginSleep started.");
    stateValue_.SetOffReason(why);
    stateValue_.SetInteractiveState(static_cast<int32_t>(InteractiveState::INTERACTIVE_STATE_BEGIN_SLEEP));
    SystemEvent systemEvent(BEGIN_SLEEP, std::to_string(why));
    SystemEventCallBack(systemEvent);
}

void ScreenLockManagerService::OnEndSleep(const int why, const int isTriggered)
{
    SCLOCK_HILOGI("ScreenLockManagerService OnEndSleep started.");
    stateValue_.SetInteractiveState(static_cast<int32_t>(InteractiveState::INTERACTIVE_STATE_END_SLEEP));
    SystemEvent systemEvent(END_SLEEP, std::to_string(why));
    SystemEventCallBack(systemEvent);
    if (stateValue_.GetScreenlockedState()) {
        NotifyUnlockListener(SCREEN_CANCEL);
    }
}

void ScreenLockManagerService::OnChangeUser(const int newUserId)
{
    SCLOCK_HILOGI("ScreenLockManagerService OnChangeUser started. newUserId %{public}d", newUserId);
    const int minUserId = 0;
    const int maxUserID = 999999999;
    if (newUserId < minUserId || newUserId >= maxUserID) {
        SCLOCK_HILOGI("ScreenLockManagerService newUserId invalid.");
        return;
    }
    stateValue_.SetCurrentUser(newUserId);
    SystemEvent systemEvent(CHANGE_USER, std::to_string(newUserId));
    SystemEventCallBack(systemEvent);
}

void ScreenLockManagerService::OnScreenlockEnabled(bool enabled)
{
    SCLOCK_HILOGI("ScreenLockManagerService OnScreenlockEnabled started.");
    stateValue_.SetScreenlockEnabled(enabled);
    SystemEvent systemEvent(SCREENLOCK_ENABLED, std::to_string(enabled));
    SystemEventCallBack(systemEvent);
}

void ScreenLockManagerService::OnExitAnimation()
{
    SCLOCK_HILOGI("ScreenLockManagerService OnExitAnimation started.");
    SystemEvent systemEvent(EXIT_ANIMATION);
    SystemEventCallBack(systemEvent);
}

int32_t ScreenLockManagerService::UnlockScreen(const sptr<ScreenLockCallbackInterface> &listener)
{
    return UnlockInner(listener);
}

int32_t ScreenLockManagerService::Unlock(const sptr<ScreenLockCallbackInterface> &listener)
{
    StartAsyncTrace(HITRACE_TAG_MISC, "ScreenLockManagerService::RequestUnlock begin", HITRACE_UNLOCKSCREEN);
    if (!IsSystemApp()) {
        SCLOCK_HILOGE("Calling app is not system app");
        return E_SCREENLOCK_NOT_SYSTEM_APP;
    }
    return UnlockInner(listener);
}

int32_t ScreenLockManagerService::UnlockInner(const sptr<ScreenLockCallbackInterface> &listener)
{
    SCLOCK_HILOGI("ScreenLockManagerService RequestUnlock started.");
    if (state_ != ServiceRunningState::STATE_RUNNING) {
        SCLOCK_HILOGE("ScreenLockManagerService RequestUnlock error.");
        return -1;
    }
    // check whether the page of app request unlock is the focus page
    if (!IsAppInForeground(IPCSkeleton::GetCallingTokenID())) {
        FinishAsyncTrace(HITRACE_TAG_MISC, "ScreenLockManagerService::RequestUnlock finish by focus",
                         HITRACE_UNLOCKSCREEN);
        SCLOCK_HILOGE("ScreenLockManagerService RequestUnlock  Unfocused.");
        return E_SCREENLOCK_NO_PERMISSION;
    }
    unlockListenerMutex_.lock();
    unlockVecListeners_.push_back(listener);
    unlockListenerMutex_.unlock();
    SystemEvent systemEvent(UNLOCKSCREEN);
    SystemEventCallBack(systemEvent, HITRACE_UNLOCKSCREEN);
    return E_SCREENLOCK_OK;
}

int32_t ScreenLockManagerService::Lock(const sptr<ScreenLockCallbackInterface> &listener)
{
    SCLOCK_HILOGI("ScreenLockManagerService RequestLock started.");
    // if (!IsSystemApp()) {
    //     SCLOCK_HILOGE("Calling app is not system app");
    //     return E_SCREENLOCK_NOT_SYSTEM_APP;
    // }
    // if (!CheckPermission("ohos.permission.ACCESS_SCREEN_LOCK_INNER")) {
    //     return E_SCREENLOCK_NO_PERMISSION;
    // }
    if (stateValue_.GetScreenlockedState()) {
        return E_SCREENLOCK_NO_PERMISSION;
    }
    lockListenerMutex_.lock();
    lockVecListeners_.push_back(listener);
    lockListenerMutex_.unlock();

    SystemEvent systemEvent(LOCKSCREEN);
    SystemEventCallBack(systemEvent, HITRACE_LOCKSCREEN);
    return E_SCREENLOCK_OK;
}

int32_t ScreenLockManagerService::IsLocked(bool &isLocked)
{
    if (!IsSystemApp()) {
        SCLOCK_HILOGE("Calling app is not system app");
        return E_SCREENLOCK_NOT_SYSTEM_APP;
    }
    isLocked = IsScreenLocked();
    return E_SCREENLOCK_OK;
}

bool ScreenLockManagerService::IsScreenLocked()
{
    if (state_ != ServiceRunningState::STATE_RUNNING) {
        SCLOCK_HILOGE("ScreenLockManagerService IsScreenLocked error.");
        return false;
    }
    bool isScreenLocked = stateValue_.GetScreenlockedState();
    SCLOCK_HILOGD("IsScreenLocked = %{public}d", isScreenLocked);
    return isScreenLocked;
}

bool ScreenLockManagerService::GetSecure()
{
    if (state_ != ServiceRunningState::STATE_RUNNING) {
        SCLOCK_HILOGE("GetSecure error.");
        return false;
    }
    SCLOCK_HILOGI("ScreenLockManagerService GetSecure started.");
    int callingUid = IPCSkeleton::GetCallingUid();
    SCLOCK_HILOGD("ScreenLockManagerService::GetSecure callingUid=%{public}d", callingUid);
    int userId = 0;
    AccountSA::OsAccountManager::GetOsAccountLocalIdFromUid(callingUid, userId);
    SCLOCK_HILOGD("userId=%{public}d", userId);
    auto getInfoCallback = std::make_shared<ScreenLockGetInfoCallback>();
    int32_t result = UserIdmClient::GetInstance().GetCredentialInfo(userId, AuthType::PIN, getInfoCallback);
    SCLOCK_HILOGI("GetCredentialInfo AuthType::PIN result = %{public}d", result);
    if (result == static_cast<int32_t>(ResultCode::SUCCESS) && getInfoCallback->IsSecure()) {
        return true;
    }
    result = UserIdmClient::GetInstance().GetCredentialInfo(userId, AuthType::FACE, getInfoCallback);
    SCLOCK_HILOGI("GetCredentialInfo AuthType::FACE result = %{public}d", result);
    if (result == static_cast<int32_t>(ResultCode::SUCCESS) && getInfoCallback->IsSecure()) {
        return true;
    }
    return false;
}

int32_t ScreenLockManagerService::OnSystemEvent(const sptr<ScreenLockSystemAbilityInterface> &listener)
{
    SCLOCK_HILOGI("ScreenLockManagerService::OnSystemEvent started.");
    if (!IsSystemApp()) {
        SCLOCK_HILOGE("Calling app is not system app");
        return E_SCREENLOCK_NOT_SYSTEM_APP;
    }
    if (!CheckPermission("ohos.permission.ACCESS_SCREEN_LOCK_INNER")) {
        return E_SCREENLOCK_NO_PERMISSION;
    }
    std::lock_guard<std::mutex> lck(listenerMutex_);
    systemEventListener_ = listener;
    SCLOCK_HILOGI("ScreenLockManagerService::OnSystemEvent end.");
    return E_SCREENLOCK_OK;
}

int32_t ScreenLockManagerService::SendScreenLockEvent(const std::string &event, int param)
{
    SCLOCK_HILOGI("ScreenLockManagerService SendScreenLockEvent started.");
    if (!IsSystemApp()) {
        SCLOCK_HILOGE("Calling app is not system app");
        return E_SCREENLOCK_NOT_SYSTEM_APP;
    }
    if (!CheckPermission("ohos.permission.ACCESS_SCREEN_LOCK_INNER")) {
        return E_SCREENLOCK_NO_PERMISSION;
    }
    SCLOCK_HILOGI("event=%{public}s ,param=%{public}d", event.c_str(), param);
    int stateResult = param;
    if (event == UNLOCK_SCREEN_RESULT) {
        UnlockScreenEvent(stateResult);
    } else if (event == SCREEN_DRAWDONE) {
        SetScreenlocked(true);
        DisplayManager::GetInstance().NotifyDisplayEvent(DisplayEvent::KEYGUARD_DRAWN);
    } else if (event == LOCK_SCREEN_RESULT) {
        LockScreenEvent(stateResult);
    }
    return E_SCREENLOCK_OK;
}

void ScreenLockManagerService::SetScreenlocked(bool isScreenlocked)
{
    SCLOCK_HILOGI("ScreenLockManagerService SetScreenlocked started.");
    stateValue_.SetScreenlocked(isScreenlocked);
}

void StateValueTest::Reset()
{
    isScreenlocked_ = true;
    screenlockEnabled_ = true;
    currentUser_ = USER_NULL;
}

void ScreenLockManagerService::PublishEvent(const std::string &eventAction)
{
    AAFwk::Want want;
    want.SetAction(eventAction);
    EventFwk::CommonEventData commonData(want);
    bool ret = EventFwk::CommonEventManager::PublishCommonEvent(commonData);
    SCLOCK_HILOGD("Publish event result is:%{public}d", ret);
}

void ScreenLockManagerService::LockScreenEvent(int stateResult)
{
    SCLOCK_HILOGD("ScreenLockManagerService LockScreenEvent stateResult:%{public}d", stateResult);
    if (stateResult == ScreenChange::SCREEN_SUCC) {
        SetScreenlocked(true);
        DisplayManager::GetInstance().NotifyDisplayEvent(DisplayEvent::KEYGUARD_DRAWN);
    } else if (stateResult == ScreenChange::SCREEN_FAIL || stateResult == ScreenChange::SCREEN_CANCEL) {
        SetScreenlocked(false);
    }
    std::lock_guard<std::mutex> autoLock(lockListenerMutex_);
    if (lockVecListeners_.size()) {
        auto callback = [this, stateResult]() {
            std::lock_guard<std::mutex> guard(lockListenerMutex_);
            for (size_t i = 0; i < lockVecListeners_.size(); i++) {
                lockVecListeners_[i]->OnCallBack(stateResult);
            }
            lockVecListeners_.clear();
        };
        serviceHandler_->PostTask(callback, INTERVAL_ZERO);
    }
    if (stateResult == ScreenChange::SCREEN_SUCC) {
        PublishEvent(EventFwk::CommonEventSupport::COMMON_EVENT_SCREEN_LOCKED);
    }
}

void ScreenLockManagerService::UnlockScreenEvent(int stateResult)
{
    SCLOCK_HILOGD("ScreenLockManagerService UnlockScreenEvent stateResult:%{public}d", stateResult);
    if (stateResult == ScreenChange::SCREEN_SUCC) {
        SetScreenlocked(false);
        DisplayManager::GetInstance().NotifyDisplayEvent(DisplayEvent::UNLOCK);
    } else if (stateResult == SCREEN_FAIL || stateResult == SCREEN_CANCEL) {
        SetScreenlocked(true);
    }
    NotifyUnlockListener(stateResult);
    if (stateResult == ScreenChange::SCREEN_SUCC) {
        PublishEvent(EventFwk::CommonEventSupport::COMMON_EVENT_SCREEN_UNLOCKED);
    }
}

void ScreenLockManagerService::SystemEventCallBack(const SystemEvent &systemEvent, TraceTaskId traceTaskId)
{
    SCLOCK_HILOGI("OnCallBack eventType is %{public}s, params is %{public}s", systemEvent.eventType_.c_str(),
        systemEvent.params_.c_str());
    if (systemEventListener_ == nullptr) {
        SCLOCK_HILOGE("systemEventListener_ is nullptr.");
        return;
    }
    auto callback = [this, systemEvent, traceTaskId]() {
        if (traceTaskId != HITRACE_BUTT) {
            StartAsyncTrace(HITRACE_TAG_MISC, "ScreenLockManagerService::" + systemEvent.eventType_ + "begin callback",
                traceTaskId);
        }
        std::lock_guard<std::mutex> lck(listenerMutex_);
        systemEventListener_->OnCallBack(systemEvent);
        if (traceTaskId != HITRACE_BUTT) {
            FinishAsyncTrace(
                HITRACE_TAG_MISC, "ScreenLockManagerService::" + systemEvent.eventType_ + "end callback", traceTaskId);
        }
    };
    if (serviceHandler_ != nullptr) {
        serviceHandler_->PostTask(callback, INTERVAL_ZERO);
    }
}

void ScreenLockManagerService::NotifyUnlockListener(const int32_t screenLockResult)
{
    std::lock_guard<std::mutex> autoLock(unlockListenerMutex_);
    if (unlockVecListeners_.size()) {
        auto callback = [this, screenLockResult]() {
            std::lock_guard<std::mutex> guard(unlockListenerMutex_);
            for (size_t i = 0; i < unlockVecListeners_.size(); i++) {
                unlockVecListeners_[i]->OnCallBack(screenLockResult);
            }
            unlockVecListeners_.clear();
        };
        serviceHandler_->PostTask(callback, INTERVAL_ZERO);
    }
}

#ifdef OHOS_TEST_FLAG
bool ScreenLockManagerService::IsAppInForeground(uint32_t tokenId)
{
    return true;
}

bool ScreenLockManagerService::IsSystemApp()
{
    return true;
}

bool ScreenLockManagerService::CheckPermission(const std::string &permissionName)
{
    return true;
}

#else
bool ScreenLockManagerService::IsAppInForeground(uint32_t tokenId)
{
    using namespace OHOS::AAFwk;
    AppInfo appInfo;
    auto ret = ScreenLockAppInfo::GetAppInfoByToken(tokenId, appInfo);
    if (!ret || appInfo.bundleName.empty()) {
        SCLOCK_HILOGE("get bundle name by token failed");
        return false;
    }
    auto elementName = AbilityManagerClient::GetInstance()->GetTopAbility();
    SCLOCK_HILOGD(" TopelementName:%{public}s, elementName.GetBundleName:%{public}s",
                  elementName.GetBundleName().c_str(), appInfo.bundleName.c_str());
    return elementName.GetBundleName() == appInfo.bundleName;
}

bool ScreenLockManagerService::IsSystemApp()
{
    return TokenIdKit::IsSystemAppByFullTokenID(IPCSkeleton::GetCallingFullTokenID());
}

bool ScreenLockManagerService::CheckPermission(const std::string &permissionName)
{
    AccessTokenID callerToken = IPCSkeleton::GetCallingTokenID();
    auto tokenType = AccessTokenKit::GetTokenTypeFlag(callerToken);
    if (tokenType != TOKEN_NATIVE && tokenType != TOKEN_SHELL && tokenType != TOKEN_HAP) {
        SCLOCK_HILOGE("check permission tokenType illegal");
        return false;
    }
    int result = AccessTokenKit::VerifyAccessToken(callerToken, permissionName);
    if (result != PERMISSION_GRANTED) {
        SCLOCK_HILOGE("check permission failed.");
        return false;
    }
    return true;
}

#endif
} // namespace ScreenLock
} // namespace OHOS