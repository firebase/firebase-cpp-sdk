// Copyright 2022 Google LLC
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

#ifndef FIREBASE_APP_CHECK_SRC_ANDROID_APP_CHECK_ANDROID_H_
#define FIREBASE_APP_CHECK_SRC_ANDROID_APP_CHECK_ANDROID_H_

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>  // NOLINT
#include <utility>
#include <vector>

#include "app/src/app_state.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/service_interfaces.h"
#include "app/src/util_android.h"
#include "app_check/src/include/firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

class AppCheckInternal : public ::firebase::internal::Service {
 public:
  explicit AppCheckInternal(::firebase::internal::AppState& app_state);

  ~AppCheckInternal() override;

  int teardown_priority() const override {
    return ::firebase::internal::kProviderServiceTeardownPriority;
  }

  ::firebase::App app() const;

  static void SetAppCheckProviderFactory(AppCheckProviderFactory* factory);

  void SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled);

  Future<AppCheckToken> GetAppCheckToken(bool force_refresh);

  Future<AppCheckToken> GetAppCheckTokenLastResult();

  Future<AppCheckToken> GetLimitedUseAppCheckToken();

  Future<AppCheckToken> GetLimitedUseAppCheckTokenLastResult();

  void AddAppCheckListener(AppCheckListener* listener);

  void RemoveAppCheckListener(AppCheckListener* listener);

  void NotifyTokenChanged(AppCheckToken token);

  FutureManager& future_manager() { return future_manager_; }

  ReferenceCountedFutureImpl* future();

 private:
  ::firebase::internal::AppState& app_state_;

  // A Java FirebaseAppCheck instance
  jobject app_check_impl_ = nullptr;

  // A Java AppCheckListener instance
  jobject j_app_check_listener_ = nullptr;

  // A collection of C++ AppCheckListeners
  std::vector<AppCheckListener*> listeners_;

  // Lock object for accessing listeners_.
  Mutex listeners_mutex_;

  // Leaf lock + CV tracking in-flight unlocked listener callbacks so
  // RemoveAppCheckListener() waits for callbacks on other threads without
  // deadlocking same-thread re-entrant removal. Lock order: listeners_mutex_ ->
  // listener_dispatch_mutex_.
  std::mutex listener_dispatch_mutex_;
  std::condition_variable listener_dispatch_cv_;
  std::vector<std::pair<AppCheckListener*, std::thread::id>>
      active_listener_callbacks_;

  struct ActiveListenerGuard {
    AppCheckInternal* owner;
    AppCheckListener* listener;
    ~ActiveListenerGuard() {
      if (owner && listener) {
        owner->EndListenerCallback(listener);
      }
    }
  };

  void BeginListenerCallbackLocked(AppCheckListener* listener);
  void EndListenerCallback(AppCheckListener* listener);
  void WaitForListenerCallbacks(AppCheckListener* listener);
  void WaitForAllOtherThreadCallbacks();

  FutureManager future_manager_;

  // String to be used when registering for JNI task callbacks.
  std::string jni_task_id_;

  // Opaque handles in NativeHandleTables for safe JNI bridge callbacks.
  int64_t app_state_handle_id_ = 0;
  int64_t listener_handle_id_ = 0;
  bool initialized_classes_ = false;
};

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_ANDROID_APP_CHECK_ANDROID_H_
