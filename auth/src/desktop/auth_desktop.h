// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_AUTH_SRC_DESKTOP_AUTH_DESKTOP_H_
#define FIREBASE_AUTH_SRC_DESKTOP_AUTH_DESKTOP_H_

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include "app/rest/request.h"
#include "app/src/scheduler.h"
#include "app/src/semaphore.h"
#include "app/src/thread.h"
#include "app/src/time.h"
#include "auth/src/data.h"
#include "auth/src/desktop/user_desktop.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/credential.h"

namespace firebase {
namespace auth {

class IdTokenRefreshListener : public IdTokenListener {
 public:
  IdTokenRefreshListener();
  ~IdTokenRefreshListener() override;

  void OnIdTokenChanged(Auth* auth) override;
  std::string GetCurrentToken();
  uint64_t GetTokenTimestamp();

 private:
  Mutex mutex_;
  uint64_t token_timestamp_;
  std::string current_token_;
};

class IdTokenRefreshThread {
 public:
  IdTokenRefreshThread();

  void Initialize(AuthData* auth_data);
  void Destroy();
  void EnableAuthRefresh();
  void DisableAuthRefresh();
  void WakeThread();

  std::string CurrentAuthToken() {
    return token_refresh_listener_.GetCurrentToken();
  }

  bool is_shutting_down() {
    MutexLock lock(ref_count_mutex_);
    return is_shutting_down_;
  }

  void set_is_shutting_down(bool value) {
    MutexLock lock(ref_count_mutex_);
    is_shutting_down_ = value;
  }

 private:
  int ref_count_;
  bool is_shutting_down_;
  Mutex ref_count_mutex_;
  Semaphore wakeup_sem_;
  IdTokenRefreshListener token_refresh_listener_;
  firebase::Thread thread_;
  Auth* auth;
};

struct AuthResultCompletionHandle {
  AuthResultCompletionHandle(const SafeFutureHandle<AuthResult>& handle,
                             AuthData* auth_data)
      : future_handle(handle), auth_data(auth_data) {}
  AuthResultCompletionHandle() = delete;
  virtual ~AuthResultCompletionHandle() { auth_data = nullptr; }

  SafeFutureHandle<AuthResult> future_handle;
  AuthData* auth_data;
};

using FunctionRegistryCallback = void (*)(void*);

class FunctionRegistryAuthStateListener : public AuthStateListener {
 public:
  void AddListener(FunctionRegistryCallback callback, void* context);
  void RemoveListener(FunctionRegistryCallback callback, void* context);
  void OnAuthStateChanged(Auth* auth) override;

 private:
  using Entry = std::pair<FunctionRegistryCallback, void*>;
  Mutex mutex_;
  std::vector<Entry> callbacks_;
};

struct AuthImpl {
  std::string api_key;
  std::string app_name;
  IdTokenRefreshThread token_refresh_thread;
  std::unique_ptr<UserDataPersist> user_data_persist;
  std::unique_ptr<FunctionRegistryAuthStateListener> internal_listeners;
  scheduler::Scheduler scheduler_;
  Mutex provider_mutex;
  std::string language_code;
  std::string assigned_emulator_url;

  std::string GetAssignedEmulatorUrl() {
    MutexLock lock(provider_mutex);
    return assigned_emulator_url;
  }
};

const int kMinutesPerTokenRefresh = 58;
const int kMsPerTokenRefresh =
    kMinutesPerTokenRefresh * ::firebase::internal::kMillisecondsPerMinute;
const int kMinRetryBackoffMs = 30 * 1000;
const int kMaxRetryBackoffMs = 16 * 60 * 1000;

class ExponentialBackoff {
 public:
  explicit ExponentialBackoff(int min_delay_ms = kMinRetryBackoffMs,
                              int max_delay_ms = kMaxRetryBackoffMs,
                              double multiplier = 2.0)
      : min_delay_ms_(std::max(0, min_delay_ms)),
        max_delay_ms_(std::max(min_delay_ms_, max_delay_ms)),
        multiplier_(std::max(1.0, multiplier)),
        current_delay_ms_(min_delay_ms_) {}

  void Reset() { current_delay_ms_ = min_delay_ms_; }

  int NextDelayMs() {
    int delay = current_delay_ms_;
    current_delay_ms_ =
        (static_cast<double>(current_delay_ms_) * multiplier_ >=
         static_cast<double>(max_delay_ms_))
            ? max_delay_ms_
            : static_cast<int>(current_delay_ms_ * multiplier_);
    return delay;
  }

  int current_delay_ms() const { return current_delay_ms_; }

 private:
  int min_delay_ms_;
  int max_delay_ms_;
  double multiplier_;
  int current_delay_ms_;
};

void InitializeUserDataPersist(AuthData* auth_data);
void DestroyUserDataPersist(AuthData* auth_data);
void LoadFinishTriggerListeners(AuthData* auth_data);

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DESKTOP_AUTH_DESKTOP_H_
