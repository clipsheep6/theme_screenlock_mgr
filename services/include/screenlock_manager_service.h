/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef OHOS_ROSEN_WINDOW_SCENE_SCREEN_LOCK_MANAGER_H
#define OHOS_ROSEN_WINDOW_SCENE_SCREEN_LOCK_MANAGER_H

#include "common/include/message_scheduler.h"
#include "session/screen/include/screen_session.h"
#include "screenlock_manager_stub.h"
#include "singleton_delegator.h"


#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "dm_common.h"
#include "event_handler.h"
#include "screenlock_callback_interface.h"
#include "screenlock_manager_stub.h"
#include "screenlock_system_ability_interface.h"

namespace OHOS::ScreenLock {
enum class ServiceRunningState { STATE_NOT_START, STATE_RUNNING };
class StateValueTest {
public:
    StateValueTest(){};
    ~StateValueTest(){};

    void Reset();

    void SetScreenlocked(bool isScreenlocked)
    {
        isScreenlocked_ = isScreenlocked;
    };

    void SetScreenlockEnabled(bool screenlockEnabled)
    {
        screenlockEnabled_ = screenlockEnabled;
    };

    void SetScreenState(int32_t screenState)
    {
        screenState_ = screenState;
    };

    void SetOffReason(int32_t offReason)
    {
        offReason_ = offReason;
    };

    void SetCurrentUser(int32_t currentUser)
    {
        currentUser_ = currentUser;
    };

    void SetInteractiveState(int32_t interactiveState)
    {
        interactiveState_ = interactiveState;
    };

    bool GetScreenlockedState()
    {
        return isScreenlocked_;
    };

    bool GetScreenlockEnabled()
    {
        return screenlockEnabled_;
    };

    int32_t GetScreenState()
    {
        return screenState_;
    };

    int32_t GetOffReason()
    {
        return offReason_;
    };

    int32_t GetCurrentUser()
    {
        return currentUser_;
    };

    int32_t GetInteractiveState()
    {
        return interactiveState_;
    };

private:
    std::atomic<bool> isScreenlocked_ = false;
    std::atomic<bool> screenlockEnabled_ = false;
    std::atomic<int32_t> offReason_ = 0;
    std::atomic<int32_t> currentUser_ = 0;
    std::atomic<int32_t> screenState_ = 0;
    std::atomic<int32_t> interactiveState_ = 0;
};

enum class ScreenState : int32_t {
    SCREEN_STATE_BEGIN_OFF = 0,
    SCREEN_STATE_END_OFF = 1,
    SCREEN_STATE_BEGIN_ON = 2,
    SCREEN_STATE_END_ON = 3,
};

enum class InteractiveState : int32_t {
    INTERACTIVE_STATE_END_SLEEP = 0,
    INTERACTIVE_STATE_BEGIN_WAKEUP = 1,
    INTERACTIVE_STATE_END_WAKEUP = 2,
    INTERACTIVE_STATE_BEGIN_SLEEP = 3,
};

class ScreenLockManagerService : public ScreenLockManagerStub {
public:
    static ScreenLockManagerService& GetInstance();
    ScreenLockManagerService(const ScreenLockManagerService&) = delete;
    ScreenLockManagerService(ScreenLockManagerService&&) = delete;
    ScreenLockManagerService& operator=(const ScreenLockManagerService&) = delete;
    ScreenLockManagerService& operator=(ScreenLockManagerService&&) = delete;

    int32_t OnSystemEvent(const sptr<ScreenLockSystemAbilityInterface> &listener) override;
    int32_t SendScreenLockEvent(const std::string &event, int param) override;
    int32_t IsLocked(bool &isLocked) override;
    bool IsScreenLocked() override;
    bool GetSecure() override;
    int32_t Unlock(const sptr<ScreenLockCallbackInterface> &listener) override;
    int32_t UnlockScreen(const sptr<ScreenLockCallbackInterface> &listener) override;
    int32_t Lock(const sptr<ScreenLockCallbackInterface> &listener) override;
    void SetScreenlocked(bool isScreenlocked);
    void RegisterDisplayPowerEventListener(int32_t times);
    void RegisterDMS();
    StateValueTest &GetState()
    {
        return stateValue_;
    }
    class ScreenLockDisplayPowerEventListener : public Rosen::IDisplayPowerEventListener {
    public:
        void OnDisplayPowerEvent(Rosen::DisplayPowerEvent event, Rosen::EventStatus status) override;
    };

protected:
    ScreenLockManagerService();
    virtual ~ScreenLockManagerService();

private:
    void OnBeginScreenOn();
    void OnEndScreenOn();
    void OnBeginScreenOff();
    void OnEndScreenOff();
    void OnBeginWakeUp();
    void OnEndWakeUp();
    void OnBeginSleep(const int why);
    void OnEndSleep(const int why, const int isTriggered);
    void OnChangeUser(const int newUserId);
    void OnScreenlockEnabled(bool enabled);
    void OnExitAnimation();
    void OnSystemReady();
    void RegisterDumpCommand();
    int32_t Init();
    void InitServiceHandler();
    void LockScreenEvent(int stateResult);
    void UnlockScreenEvent(int stateResult);
    void SystemEventCallBack(const SystemEvent &systemEvent, TraceTaskId traceTaskId = HITRACE_BUTT);
    int32_t UnlockInner(const sptr<ScreenLockCallbackInterface> &listener);
    void PublishEvent(const std::string &eventAction);
    bool IsAppInForeground(uint32_t tokenId);
    bool IsSystemApp();
    bool CheckPermission(const std::string &permissionName);
    void NotifyUnlockListener(const int32_t screenLockResult);

    ServiceRunningState state_;
    static std::shared_ptr<AppExecFwk::EventHandler> serviceHandler_;
    sptr<Rosen::IDisplayPowerEventListener> displayPowerEventListener_;
    std::mutex listenerMutex_;
    sptr<ScreenLockSystemAbilityInterface> systemEventListener_;
    std::mutex unlockListenerMutex_;
    std::vector<sptr<ScreenLockCallbackInterface>> unlockVecListeners_;
    std::mutex lockListenerMutex_;
    std::vector<sptr<ScreenLockCallbackInterface>> lockVecListeners_;
    StateValueTest stateValue_;
    const int32_t startTime_ = 1900;
    const int32_t extraMonth_ = 1;
    bool flag_ = false;
};
} // namespace OHOS::Rosen

#endif // OHOS_ROSEN_WINDOW_SCENE_SCREEN_LOCK_MANAGER_H