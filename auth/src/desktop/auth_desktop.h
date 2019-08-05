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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_DESKTOP_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_DESKTOP_H_

#include <memory>

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

// Token listener used by the IdTokenRefreshThread object.  Basically just
// listens for changes, and when one occurs, caches the result, along with a
// timestamp.  All functions are thread-safe and locked with the mutex.
class IdTokenRefreshListener : public IdTokenListener {
 public:
  IdTokenRefreshListener();
  ~IdTokenRefreshListener();

  void OnIdTokenChanged(Auth* auth) override;
  std::string GetCurrentToken();
  uint64_t GetTokenTimestamp();

 private:
  // Guards all members of this class.
  Mutex mutex_;
  uint64_t token_timestamp_;
  std::string current_token_;
};

// This class handles the full lifecycle of the token refresh thread.  It
// manages thread creation, deletion, activation, deactivation, etc.  It also
// owns the IdTokenRefreshListener object used by the thread to detect changes
// to the auth token.
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

  // Thread-safe getter, for checking if it's time to shut down.
  bool is_shutting_down() {
    bool result;
    {
      MutexLock lock(ref_count_mutex_);
      result = is_shutting_down_;
    }
    return result;
  }

  // Thread-safe setter.
  void set_is_shutting_down(bool value) {
    MutexLock lock(ref_count_mutex_);
    is_shutting_down_ = value;
  }

 private:
  int ref_count_;
  bool is_shutting_down_;

  // Mutex used by the refresher thread when updating refcounts.
  Mutex ref_count_mutex_;

  // Semaphore used for waking up the thread, when it is asleep.
  Semaphore wakeup_sem_;

  IdTokenRefreshListener token_refresh_listener_;

  firebase::Thread thread_;
  Auth* auth;
};

// Facilitates completion of Federated Auth operations on non-mobile
// environments. Custom application logic fulfills the authentication request
// and uses this completion handle in callbacks. Our callbacks observe
// contextual information in these handles to access to trigger the
// corresponding Future<SignInResult>.
struct AuthCompletionHandle {
 public:
  AuthCompletionHandle(const SafeFutureHandle<SignInResult>& handle,
                       AuthData* auth_data)
      : future_handle(handle), auth_data(auth_data) {}

  AuthCompletionHandle() = delete;

  virtual ~AuthCompletionHandle() {
    auth_data = nullptr;
  }

  SafeFutureHandle<SignInResult> future_handle;
  AuthData* auth_data;
};

// The desktop-specific Auth implementation.
struct AuthImpl {
  AuthImpl() {}

  // The application's API key.
  std::string api_key;

  // The app name, which is programmatically set. This is not part of app
  // options.
  std::string app_name;

  // Thread responsible for refreshing the auth token periodically.
  IdTokenRefreshThread token_refresh_thread;
  // Instance responsible for user data persistence.
  UniquePtr<UserDataPersist> user_data_persist;

  // Serializes all REST call from this object.
  scheduler::Scheduler scheduler_;

  // Synchronization primative for tracking sate of FederatedAuth futures.
  Mutex provider_mutex;
};

// Constant, describing how often we automatically fetch a new auth token.
// Auth tokens expire after 60 minutes so we refresh slightly before.
const int kMinutesPerTokenRefresh = 58;
const int kMsPerTokenRefresh =
    kMinutesPerTokenRefresh * internal::kMillisecondsPerMinute;

void InitializeUserDataPersist(AuthData* auth_data);
void DestroyUserDataPersist(AuthData* auth_data);
void LoadFinishTriggerListeners(AuthData* auth_data);

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_DESKTOP_H_
