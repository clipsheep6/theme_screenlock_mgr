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
#include "screenlock_system_ability.h"

#include <cerrno>
#include <ctime>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <string>
#include <sys/time.h>
#include <unistd.h>
#include <memory>

#include "common_event_support.h"
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
#include "system_ability.h"
#include "system_ability_definition.h"
#include "tokenid_kit.h"
#include "user_idm_client.h"
#include "want.h"
#include "xcollie/watchdog.h"
#include "scene_board_judgement.h"
#include "window_manager.h"

namespace OHOS {
namespace ScreenLock {
using namespace std;
using namespace OHOS::HiviewDFX;
using namespace OHOS::Rosen;
using namespace OHOS::UserIam::UserAuth;
using namespace OHOS::Security::AccessToken;

const bool REGISTER_RESULT = Rosen::SceneBoardJudgement::IsSceneBoardEnabled() ? false :
    SystemAbility::MakeAndRegisterAbility(new ScreenLockSystemAbility(SCREENLOCK_SERVICE_ID, true));
const std::int64_t TIME_OUT_MILLISECONDS = 10000L;
const std::int64_t INIT_INTERVAL = 5000000L;
const std::int64_t DELAY_TIME = 1000000L;
std::mutex ScreenLockSystemAbility::instanceLock_;
sptr<ScreenLockSystemAbility> ScreenLockSystemAbility::instance_;
constexpr int32_t MAX_RETRY_TIMES = 20;
std::shared_ptr<ffrt::queue> ScreenLockSystemAbility::queue_;
ScreenLockSystemAbility::ScreenLockSystemAbility(int32_t systemAbilityId, bool runOnCreate)
    : SystemAbility(systemAbilityId, runOnCreate), state_(ServiceRunningState::STATE_NOT_START)
{
}

ScreenLockSystemAbility::~ScreenLockSystemAbility()
{
    SCLOCK_HILOGD("~ScreenLockSystemAbility state_  is %{public}d.", static_cast<int>(state_));
    if (Rosen::SceneBoardJudgement::IsSceneBoardEnabled()) {
        queue_ = nullptr;
        instance_ = nullptr;
        DisplayManager::GetInstance().UnregisterDisplayPowerEventListener(displayPowerEventListener_);
        displayPowerEventListener_ = nullptr;
    }
}

sptr<ScreenLockSystemAbility> ScreenLockSystemAbility::GetInstance()
{
    if (instance_ == nullptr) {
        std::lock_guard<std::mutex> autoLock(instanceLock_);
        if (instance_ == nullptr) {
            SCLOCK_HILOGI("ScreenLockSystemAbility create instance.");
            instance_ = new ScreenLockSystemAbility(SCREENLOCK_SERVICE_ID, true);
            instance_->Initialize();
        }
    }
    return instance_;
}

int32_t ScreenLockSystemAbility::Init()
{
    bool ret = Publish(ScreenLockSystemAbility::GetInstance());
    if (!ret) {
        SCLOCK_HILOGE("Publish ScreenLockSystemAbility failed.");
        return E_SCREENLOCK_PUBLISH_FAIL;
    }
    SCLOCK_HILOGD("state_ is %{public}d.", static_cast<int>(state_));
    stateValue_.Reset();
    SCLOCK_HILOGI("Init ScreenLockSystemAbility success.");
    return ERR_OK;
}

void ScreenLockSystemAbility::OnStart()
{
    SCLOCK_HILOGI("ScreenLockSystemAbility::Enter OnStart.");
    if (instance_ == nullptr) {
        instance_ = this;
    }
    if (state_ == ServiceRunningState::STATE_RUNNING) {
        SCLOCK_HILOGI("ScreenLockSystemAbility is already running.");
        return;
    }
    InitServiceHandler();
    if (Init() != ERR_OK) {
        auto callback = [=]() { Init(); };
        queue_->submit(callback, ffrt::task_attr().delay(INIT_INTERVAL));
        SCLOCK_HILOGW("ScreenLockSystemAbility Init failed. Try again 5s later");
    }
    AddSystemAbilityListener(DISPLAY_MANAGER_SERVICE_SA_ID);
    RegisterDumpCommand();
    return;
}

void ScreenLockSystemAbility::OnAddSystemAbility(int32_t systemAbilityId, const std::string &deviceId)
{
    SCLOCK_HILOGI("OnAddSystemAbility systemAbilityId:%{public}d added!", systemAbilityId);
    if (systemAbilityId == DISPLAY_MANAGER_SERVICE_SA_ID) {
        int times = 0;
        if (displayPowerEventListener_ == nullptr) {
            displayPowerEventListener_ = new ScreenLockSystemAbility::ScreenLockDisplayPowerEventListener();
        }
        RegisterDisplayPowerEventListener(times);
    }
}

void ScreenLockSystemAbility::RegisterDisplayPowerEventListener(int32_t times)
{
    times++;
    flag_ = (DisplayManager::GetInstance().RegisterDisplayPowerEventListener(displayPowerEventListener_) ==
             DMError::DM_OK);
    if (flag_ == false && times <= MAX_RETRY_TIMES) {
        SCLOCK_HILOGW("ScreenLockSystemAbility RegisterDisplayPowerEventListener failed");
        auto callback = [this, times]() { RegisterDisplayPowerEventListener(times); };
        queue_->submit(callback, ffrt::task_attr().delay(DELAY_TIME));
    } else if (flag_) {
        state_ = ServiceRunningState::STATE_RUNNING;
        auto callback = [=]() { OnSystemReady(); };
        queue_->submit(callback);
    }
    SCLOCK_HILOGI("ScreenLockSystemAbility RegisterDisplayPowerEventListener end, flag_:%{public}d, times:%{public}d",
        flag_, times);
}

void ScreenLockSystemAbility::InitServiceHandler()
{
    SCLOCK_HILOGI("InitServiceHandler started.");
    if (queue_ != nullptr) {
        SCLOCK_HILOGI("InitServiceHandler already init.");
        return;
    }
    queue_ = std::make_shared<ffrt::queue>("ScreenLockSystemAbility");
    SCLOCK_HILOGI("InitServiceHandler succeeded.");
}

void ScreenLockSystemAbility::OnStop()
{
    SCLOCK_HILOGI("OnStop started.");
    if (state_ != ServiceRunningState::STATE_RUNNING) {
        return;
    }
    queue_ = nullptr;
    instance_ = nullptr;
    state_ = ServiceRunningState::STATE_NOT_START;
    DisplayManager::GetInstance().UnregisterDisplayPowerEventListener(displayPowerEventListener_);
    SCLOCK_HILOGI("OnStop end.");
}

void ScreenLockSystemAbility::ScreenLockDisplayPowerEventListener::OnDisplayPowerEvent(DisplayPowerEvent event,
    EventStatus status)
{
    SCLOCK_HILOGI("OnDisplayPowerEvent event=%{public}d", static_cast<int>(event));
    SCLOCK_HILOGI("OnDisplayPowerEvent status= %{public}d", static_cast<int>(status));
    if (status == EventStatus::BEGIN) {
        if (event == DisplayPowerEvent::WAKE_UP) {
            instance_->OnBeginWakeUp();
        } else if (event == DisplayPowerEvent::SLEEP) {
            instance_->OnBeginSleep(0);
        } else if (event == DisplayPowerEvent::DISPLAY_ON) {
            instance_->OnBeginScreenOn();
        } else if (event == DisplayPowerEvent::DISPLAY_OFF) {
            instance_->OnBeginScreenOff();
        } else if (event == DisplayPowerEvent::DESKTOP_READY) {
            instance_->OnExitAnimation();
        }
    } else if (status == EventStatus::END) {
        if (event == DisplayPowerEvent::WAKE_UP) {
            instance_->OnEndWakeUp();
        } else if (event == DisplayPowerEvent::SLEEP) {
            instance_->OnEndSleep(0, 0);
        } else if (event == DisplayPowerEvent::DISPLAY_ON) {
            instance_->OnEndScreenOn();
        } else if (event == DisplayPowerEvent::DISPLAY_OFF) {
            instance_->OnEndScreenOff();
        }
    }
}

void ScreenLockSystemAbility::OnBeginScreenOff()
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnBeginScreenOff started.");
    stateValue_.SetScreenState(static_cast<int32_t>(ScreenState::SCREEN_STATE_BEGIN_OFF));
    SystemEvent systemEvent(BEGIN_SCREEN_OFF);
    SystemEventCallBack(systemEvent);
}

void ScreenLockSystemAbility::OnEndScreenOff()
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnEndScreenOff started.");
    stateValue_.SetScreenState(static_cast<int32_t>(ScreenState::SCREEN_STATE_END_OFF));
    SystemEvent systemEvent(END_SCREEN_OFF);
    SystemEventCallBack(systemEvent);
}

void ScreenLockSystemAbility::OnBeginScreenOn()
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnBeginScreenOn started.");
    stateValue_.SetScreenState(static_cast<int32_t>(ScreenState::SCREEN_STATE_BEGIN_ON));
    SystemEvent systemEvent(BEGIN_SCREEN_ON);
    SystemEventCallBack(systemEvent);
}

void ScreenLockSystemAbility::OnSystemReady()
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnSystemReady started.");
    bool isExitFlag = false;
    int tryTime = 20;
    int minTryTime = 0;
    while (!isExitFlag && (tryTime > minTryTime)) {
        if (systemEventListener_ != nullptr) {
            SCLOCK_HILOGI("ScreenLockSystemAbility OnSystemReady started1.");
            std::lock_guard<std::mutex> lck(listenerMutex_);
            SystemEvent systemEvent(SYSTEM_READY);
            systemEventListener_->OnCallBack(systemEvent);
            isExitFlag = true;
        } else {
            SCLOCK_HILOGE("ScreenLockSystemAbility OnSystemReady type not found., flag_ = %{public}d", flag_);
            sleep(1);
        }
        --tryTime;
    }
}

void ScreenLockSystemAbility::OnEndScreenOn()
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnEndScreenOn started.");
    stateValue_.SetScreenState(static_cast<int32_t>(ScreenState::SCREEN_STATE_END_ON));
    SystemEvent systemEvent(END_SCREEN_ON);
    SystemEventCallBack(systemEvent);
}

void ScreenLockSystemAbility::OnBeginWakeUp()
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnBeginWakeUp started.");
    stateValue_.SetInteractiveState(static_cast<int32_t>(InteractiveState::INTERACTIVE_STATE_BEGIN_WAKEUP));
    SystemEvent systemEvent(BEGIN_WAKEUP);
    SystemEventCallBack(systemEvent);
}

void ScreenLockSystemAbility::OnEndWakeUp()
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnEndWakeUp started.");
    stateValue_.SetInteractiveState(static_cast<int32_t>(InteractiveState::INTERACTIVE_STATE_END_WAKEUP));
    SystemEvent systemEvent(END_WAKEUP);
    SystemEventCallBack(systemEvent);
}

void ScreenLockSystemAbility::OnBeginSleep(const int why)
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnBeginSleep started.");
    stateValue_.SetOffReason(why);
    stateValue_.SetInteractiveState(static_cast<int32_t>(InteractiveState::INTERACTIVE_STATE_BEGIN_SLEEP));
    SystemEvent systemEvent(BEGIN_SLEEP, std::to_string(why));
    SystemEventCallBack(systemEvent);
}

void ScreenLockSystemAbility::OnEndSleep(const int why, const int isTriggered)
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnEndSleep started.");
    stateValue_.SetInteractiveState(static_cast<int32_t>(InteractiveState::INTERACTIVE_STATE_END_SLEEP));
    SystemEvent systemEvent(END_SLEEP, std::to_string(why));
    SystemEventCallBack(systemEvent);
    if (stateValue_.GetScreenlockedState()) {
        NotifyUnlockListener(SCREEN_CANCEL);
    }
}

void ScreenLockSystemAbility::OnChangeUser(const int newUserId)
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnChangeUser started. newUserId %{public}d", newUserId);
    const int minUserId = 0;
    const int maxUserID = 999999999;
    if (newUserId < minUserId || newUserId >= maxUserID) {
        SCLOCK_HILOGI("ScreenLockSystemAbility newUserId invalid.");
        return;
    }
    stateValue_.SetCurrentUser(newUserId);
    SystemEvent systemEvent(CHANGE_USER, std::to_string(newUserId));
    SystemEventCallBack(systemEvent);
}

void ScreenLockSystemAbility::OnScreenlockEnabled(bool enabled)
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnScreenlockEnabled started.");
    stateValue_.SetScreenlockEnabled(enabled);
    SystemEvent systemEvent(SCREENLOCK_ENABLED, std::to_string(enabled));
    SystemEventCallBack(systemEvent);
}

void ScreenLockSystemAbility::OnExitAnimation()
{
    SCLOCK_HILOGI("ScreenLockSystemAbility OnExitAnimation started.");
    SystemEvent systemEvent(EXIT_ANIMATION);
    SystemEventCallBack(systemEvent);
}

int32_t ScreenLockSystemAbility::UnlockScreen(const sptr<ScreenLockCallbackInterface> &listener)
{
    return UnlockInner(listener);
}

int32_t ScreenLockSystemAbility::Unlock(const sptr<ScreenLockCallbackInterface> &listener)
{
    StartAsyncTrace(HITRACE_TAG_MISC, "ScreenLockSystemAbility::RequestUnlock begin", HITRACE_UNLOCKSCREEN);
    if (!IsSystemApp()) {
        SCLOCK_HILOGE("Calling app is not system app");
        return E_SCREENLOCK_NOT_SYSTEM_APP;
    }
    return UnlockInner(listener);
}

int32_t ScreenLockSystemAbility::UnlockInner(const sptr<ScreenLockCallbackInterface> &listener)
{
    if (state_ != ServiceRunningState::STATE_RUNNING) {
        SCLOCK_HILOGI("ScreenLockSystemAbility RequestUnlock restart.");
        OnStart();
    }
    SCLOCK_HILOGI("ScreenLockSystemAbility RequestUnlock started.");

    // check whether the page of app request unlock is the focus page
    if (!IsAppInForeground(IPCSkeleton::GetCallingUid())) {
        FinishAsyncTrace(HITRACE_TAG_MISC, "ScreenLockSystemAbility::RequestUnlock finish by focus",
            HITRACE_UNLOCKSCREEN);
        SCLOCK_HILOGE("ScreenLockSystemAbility RequestUnlock  Unfocused.");
        return E_SCREENLOCK_NO_PERMISSION;
    }
    unlockListenerMutex_.lock();
    unlockVecListeners_.push_back(listener);
    unlockListenerMutex_.unlock();
    SystemEvent systemEvent(UNLOCKSCREEN);
    SystemEventCallBack(systemEvent, HITRACE_UNLOCKSCREEN);
    return E_SCREENLOCK_OK;
}

int32_t ScreenLockSystemAbility::Lock(const sptr<ScreenLockCallbackInterface> &listener)
{
    SCLOCK_HILOGI("ScreenLockSystemAbility RequestLock started.");
    if (!IsSystemApp()) {
        SCLOCK_HILOGE("Calling app is not system app");
        return E_SCREENLOCK_NOT_SYSTEM_APP;
    }
    if (!CheckPermission("ohos.permission.ACCESS_SCREEN_LOCK_INNER")) {
        return E_SCREENLOCK_NO_PERMISSION;
    }
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

int32_t ScreenLockSystemAbility::IsLocked(bool &isLocked)
{
    if (!IsSystemApp()) {
        SCLOCK_HILOGE("Calling app is not system app");
        return E_SCREENLOCK_NOT_SYSTEM_APP;
    }
    isLocked = IsScreenLocked();
    return E_SCREENLOCK_OK;
}

bool ScreenLockSystemAbility::IsScreenLocked()
{
    if (state_ != ServiceRunningState::STATE_RUNNING) {
        SCLOCK_HILOGI("IsScreenLocked restart.");
        OnStart();
    }
    bool isScreenLocked = stateValue_.GetScreenlockedState();
    SCLOCK_HILOGD("IsScreenLocked = %{public}d", isScreenLocked);
    return isScreenLocked;
}

bool ScreenLockSystemAbility::GetSecure()
{
    if (state_ != ServiceRunningState::STATE_RUNNING) {
        SCLOCK_HILOGI("ScreenLockSystemAbility GetSecure restart.");
        OnStart();
    }
    SCLOCK_HILOGI("ScreenLockSystemAbility GetSecure started.");
    int callingUid = IPCSkeleton::GetCallingUid();
    SCLOCK_HILOGD("ScreenLockSystemAbility::GetSecure callingUid=%{public}d", callingUid);
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

int32_t ScreenLockSystemAbility::OnSystemEvent(const sptr<ScreenLockSystemAbilityInterface> &listener)
{
    SCLOCK_HILOGI("ScreenLockSystemAbility::OnSystemEvent started.");
    if (!IsSystemApp()) {
        SCLOCK_HILOGE("Calling app is not system app");
        return E_SCREENLOCK_NOT_SYSTEM_APP;
    }
    if (!CheckPermission("ohos.permission.ACCESS_SCREEN_LOCK_INNER")) {
        return E_SCREENLOCK_NO_PERMISSION;
    }
    std::lock_guard<std::mutex> lck(listenerMutex_);
    systemEventListener_ = listener;
    SCLOCK_HILOGI("ScreenLockSystemAbility::OnSystemEvent end.");
    return E_SCREENLOCK_OK;
}

int32_t ScreenLockSystemAbility::SendScreenLockEvent(const std::string &event, int param)
{
    SCLOCK_HILOGI("ScreenLockSystemAbility SendScreenLockEvent started.");
    if (!IsSystemApp()) {
        SCLOCK_HILOGE("Calling app is not system app");
        return E_SCREENLOCK_NOT_SYSTEM_APP;
    }
    if (!CheckPermission("ohos.permission.ACCESS_SCREEN_LOCK_INNER")) {
        return E_SCREENLOCK_NO_PERMISSION;
    }
    SCLOCK_HILOGD("event=%{public}s ,param=%{public}d", event.c_str(), param);
    int stateResult = param;
    if (event == UNLOCK_SCREEN_RESULT) {
        UnlockScreenEvent(stateResult);
    } else if (event == SCREEN_DRAWDONE) {
        SetScreenlocked(true);
        NotifyDisplayEvent(DisplayEvent::KEYGUARD_DRAWN);
    } else if (event == LOCK_SCREEN_RESULT) {
        LockScreenEvent(stateResult);
    }
    return E_SCREENLOCK_OK;
}

void ScreenLockSystemAbility::SetScreenlocked(bool isScreenlocked)
{
    SCLOCK_HILOGI("ScreenLockSystemAbility SetScreenlocked state:%{public}d.", isScreenlocked);
    stateValue_.SetScreenlocked(isScreenlocked);
}

void StateValue::Reset()
{
    isScreenlocked_ = true;
    screenlockEnabled_ = true;
    currentUser_ = USER_NULL;
}

int ScreenLockSystemAbility::Dump(int fd, const std::vector<std::u16string> &args)
{
    int uid = static_cast<int>(IPCSkeleton::GetCallingUid());
    const int maxUid = 10000;
    if (uid > maxUid) {
        return 0;
    }

    std::vector<std::string> argsStr;
    for (auto item : args) {
        argsStr.emplace_back(Str16ToStr8(item));
    }

    DumpHelper::GetInstance().Dispatch(fd, argsStr);
    return ERR_OK;
}

void ScreenLockSystemAbility::RegisterDumpCommand()
{
    auto cmd = std::make_shared<Command>(std::vector<std::string>{ "-all" }, "dump all screenlock information",
        [this](const std::vector<std::string> &input, std::string &output) -> bool {
            bool screenLocked = stateValue_.GetScreenlockedState();
            bool screenState = stateValue_.GetScreenState();
            int32_t offReason = stateValue_.GetOffReason();
            int32_t interactiveState = stateValue_.GetInteractiveState();
            string temp_screenLocked = "";
            screenLocked ? temp_screenLocked = "true" : temp_screenLocked = "false";
            string temp_screenState = "";
            screenState ? temp_screenState = "true" : temp_screenState = "false";
            output.append("\n Screenlock system state\\tValue\\t\\tDescription\n")
                .append(" * screenLocked  \t\t" + temp_screenLocked + "\t\twhether there is lock screen status\n")
                .append(" * screenState  \t\t" + temp_screenState + "\t\tscreen on / off status\n")
                .append(" * offReason  \t\t\t" + std::to_string(offReason) + "\t\tscreen failure reason\n")
                .append(
                " * interactiveState \t\t" + std::to_string(interactiveState) + "\t\tscreen interaction status\n");
            return true;
        });
    DumpHelper::GetInstance().RegisterCommand(cmd);
}

void ScreenLockSystemAbility::PublishEvent(const std::string &eventAction)
{
    AAFwk::Want want;
    want.SetAction(eventAction);
    EventFwk::CommonEventData commonData(want);
    bool ret = EventFwk::CommonEventManager::PublishCommonEvent(commonData);
    SCLOCK_HILOGD("Publish event result is:%{public}d", ret);
}

void ScreenLockSystemAbility::LockScreenEvent(int stateResult)
{
    SCLOCK_HILOGD("ScreenLockSystemAbility LockScreenEvent stateResult:%{public}d", stateResult);
    if (stateResult == ScreenChange::SCREEN_SUCC) {
        SetScreenlocked(true);
        NotifyDisplayEvent(DisplayEvent::KEYGUARD_DRAWN);
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
        queue_->submit(callback);
    }
    if (stateResult == ScreenChange::SCREEN_SUCC) {
        PublishEvent(EventFwk::CommonEventSupport::COMMON_EVENT_SCREEN_LOCKED);
    }
}

void ScreenLockSystemAbility::UnlockScreenEvent(int stateResult)
{
    SCLOCK_HILOGD("ScreenLockSystemAbility UnlockScreenEvent stateResult:%{public}d", stateResult);
    if (stateResult == ScreenChange::SCREEN_SUCC) {
        SetScreenlocked(false);
        NotifyDisplayEvent(DisplayEvent::UNLOCK);
    } else if (stateResult == SCREEN_FAIL || stateResult == SCREEN_CANCEL) {
        SetScreenlocked(true);
    }
    NotifyUnlockListener(stateResult);
    if (stateResult == ScreenChange::SCREEN_SUCC) {
        PublishEvent(EventFwk::CommonEventSupport::COMMON_EVENT_SCREEN_UNLOCKED);
    }
}

void ScreenLockSystemAbility::SystemEventCallBack(const SystemEvent &systemEvent, TraceTaskId traceTaskId)
{
    SCLOCK_HILOGI("OnCallBack eventType is %{public}s, params is %{public}s", systemEvent.eventType_.c_str(),
        systemEvent.params_.c_str());
    if (systemEventListener_ == nullptr) {
        SCLOCK_HILOGE("systemEventListener_ is nullptr.");
        return;
    }
    auto callback = [this, systemEvent, traceTaskId]() {
        if (traceTaskId != HITRACE_BUTT) {
            StartAsyncTrace(HITRACE_TAG_MISC, "ScreenLockSystemAbility::" + systemEvent.eventType_ + "begin callback",
                traceTaskId);
        }
        std::lock_guard<std::mutex> lck(listenerMutex_);
        systemEventListener_->OnCallBack(systemEvent);
        if (traceTaskId != HITRACE_BUTT) {
            FinishAsyncTrace(
                HITRACE_TAG_MISC, "ScreenLockSystemAbility::" + systemEvent.eventType_ + "end callback", traceTaskId);
        }
    };
    if (queue_ != nullptr) {
        queue_->submit(callback);
    }
}

void ScreenLockSystemAbility::NotifyUnlockListener(const int32_t screenLockResult)
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
        queue_->submit(callback);
    }
}

void ScreenLockSystemAbility::Initialize()
{
    if (!Rosen::SceneBoardJudgement::IsSceneBoardEnabled()) {
        return;
    }
    InitServiceHandler();
    displayPowerEventListener_ = new ScreenLockSystemAbility::ScreenLockDisplayPowerEventListener();
    int times = 0;
    RegisterDisplayPowerEventListener(times);
    stateValue_.Reset();
}

void ScreenLockSystemAbility::NotifyDisplayEvent(DisplayEvent event)
{
    if (queue_ == nullptr) {
        SCLOCK_HILOGE("NotifyDisplayEvent queue_ is nullptr.");
        return;
    }
    auto callback = [event]() { DisplayManager::GetInstance().NotifyDisplayEvent(event); };
    queue_->submit(callback);
}

void ScreenLockSystemAbility::ResetFfrtQueue()
{
    queue_.reset();
}

#ifdef OHOS_TEST_FLAG
bool ScreenLockSystemAbility::IsAppInForeground(int32_t uid)
{
    return true;
}

bool ScreenLockSystemAbility::IsSystemApp()
{
    return true;
}

bool ScreenLockSystemAbility::CheckPermission(const std::string &permissionName)
{
    return true;
}

#else
bool ScreenLockSystemAbility::IsAppInForeground(int32_t uid)
{
#ifdef CONFIG_FACTORY_MODE
    return true;
#endif
    FocusChangeInfo focusInfo;
    WindowManager::GetInstance().GetFocusWindowInfo(focusInfo);
    SCLOCK_HILOGD("callingUid:%{public}d, focusWidnowUid:%{public}d", uid, focusInfo.uid_);
    return uid == focusInfo.uid_;
}

bool ScreenLockSystemAbility::IsSystemApp()
{
    return TokenIdKit::IsSystemAppByFullTokenID(IPCSkeleton::GetCallingFullTokenID());
}

bool ScreenLockSystemAbility::CheckPermission(const std::string &permissionName)
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