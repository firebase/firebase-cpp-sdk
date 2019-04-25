// Copyright 2019 Google LLC
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

#include "auth/src/desktop/secure/user_secure_manager.h"

#include "app/src/callback.h"
#include "auth/src/desktop/secure/user_secure_internal.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif  // __APPLE__

#if defined(_WIN32)
#include "auth/src/desktop/secure/user_secure_windows_internal.h"
#define USER_SECURE_TYPE UserSecureWindowsInternal

#elif defined(TARGET_OS_OSX) && TARGET_OS_OSX
#include "auth/src/desktop/secure/user_secure_fake_internal.h"
#define USER_SECURE_TYPE UserSecureFakeInternal

#elif defined(__linux__)
#include "auth/src/desktop/secure/user_secure_fake_internal.h"
#define USER_SECURE_TYPE UserSecureFakeInternal

#else  // Unknown platform, use fake version.
#warning "No secure storage for Auth persistence is available on this platform."
#include "auth/src/desktop/secure/user_secure_fake_internal.h"
#define USER_SECURE_TYPE UserSecureFakeInternal
#endif

namespace firebase {
namespace auth {
namespace secure {

using callback::NewCallback;

Mutex UserSecureManager::s_scheduler_mutex_;  // NOLINT
scheduler::Scheduler* UserSecureManager::s_scheduler_;
int32_t UserSecureManager::s_scheduler_ref_count_;

#if defined(_WIN32) || defined(TARGET_OS_OSX) && TARGET_OS_OSX || \
    defined(__linux__)
const char kAuthKeyName[] = "com.google.firebase.auth.Keys";
#else
// fake internal would use current folder.
const char kAuthKeyName[] = "./";
#endif

UserSecureManager::UserSecureManager()
    : future_api_(kUserSecureFnCount), safe_this_(this) {
  user_secure_ = MakeUnique<USER_SECURE_TYPE>(kAuthKeyName);
  CreateScheduler();
}

UserSecureManager::UserSecureManager(
    UniquePtr<UserSecureInternal> user_secure_internal)
    : user_secure_(std::move(user_secure_internal)),
      future_api_(kUserSecureFnCount),
      safe_this_(this) {
  CreateScheduler();
}

UserSecureManager::~UserSecureManager() {
  // Clear safe reference immediately so that scheduled callback can skip
  // executing code which requires reference to this.
  safe_this_.ClearReference();
  DestroyScheduler();
}

Future<std::string> UserSecureManager::LoadUserData(
    const std::string& app_name) {
  const auto future_handle =
      future_api_.SafeAlloc<std::string>(kUserSecureFnLoad);

  auto data_handle = MakeShared<UserSecureDataHandle<std::string>>(
      app_name, "", &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, SharedPtr<UserSecureDataHandle<std::string>> handle,
         UserSecureInternal* internal) {
        if (internal == nullptr) {
          handle->future_api->Complete(handle->future_handle, kNoInternal,
                                       "manager doesn't have valid internal");
          return;
        }
        ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          std::string result = internal->LoadUserData(handle->app_name);
          std::string empty_str("");
          if (result.empty()) {
            std::string message("No entry for key:" + handle->app_name);
            handle->future_api->CompleteWithResult(
                handle->future_handle, kNoEntry, message.c_str(), empty_str);
          } else {
            handle->future_api->CompleteWithResult(handle->future_handle,
                                                   kSuccess, "", result);
          }
        }
      },
      safe_this_, data_handle, user_secure_.get());
  s_scheduler_->Schedule(callback);
  return MakeFuture(&future_api_, future_handle);
}

Future<void> UserSecureManager::SaveUserData(const std::string& app_name,
                                             const std::string& user_data) {
  const auto future_handle = future_api_.SafeAlloc<void>(kUserSecureFnSave);

  auto data_handle = MakeShared<UserSecureDataHandle<void>>(
      app_name, user_data, &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, SharedPtr<UserSecureDataHandle<void>> handle,
         UserSecureInternal* internal) {
        if (internal == nullptr) {
          handle->future_api->Complete(handle->future_handle, kNoInternal,
                                       "manager doesn't have valid internal");
          return;
        }
        ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          internal->SaveUserData(handle->app_name, handle->user_data);
          handle->future_api->Complete(handle->future_handle, kSuccess);
        }
      },
      safe_this_, data_handle, user_secure_.get());
  s_scheduler_->Schedule(callback);
  return MakeFuture(&future_api_, future_handle);
}

Future<void> UserSecureManager::DeleteUserData(const std::string& app_name) {
  const auto future_handle = future_api_.SafeAlloc<void>(kUserSecureFnDelete);

  auto data_handle = MakeShared<UserSecureDataHandle<void>>(
      app_name, "", &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, SharedPtr<UserSecureDataHandle<void>> handle,
         UserSecureInternal* internal) {
        if (internal == nullptr) {
          handle->future_api->Complete(handle->future_handle, kNoInternal,
                                       "manager doesn't have valid internal");
          return;
        }
        ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          internal->DeleteUserData(handle->app_name);
          handle->future_api->Complete(handle->future_handle, kSuccess);
        }
      },
      safe_this_, data_handle, user_secure_.get());
  s_scheduler_->Schedule(callback);
  return MakeFuture(&future_api_, future_handle);
}

Future<void> UserSecureManager::DeleteAllData() {
  auto future_handle = future_api_.SafeAlloc<void>(kUserSecureFnDeleteAll);

  auto data_handle = MakeShared<UserSecureDataHandle<void>>(
      "", "", &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, SharedPtr<UserSecureDataHandle<void>> handle,
         UserSecureInternal* internal) {
        if (internal == nullptr) {
          handle->future_api->Complete(handle->future_handle, kNoInternal,
                                       "manager doesn't have valid internal");
          return;
        }
        ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          internal->DeleteAllData();
          handle->future_api->Complete(handle->future_handle, kSuccess);
        }
      },
      safe_this_, data_handle, user_secure_.get());
  s_scheduler_->Schedule(callback);
  return MakeFuture(&future_api_, future_handle);
}

void UserSecureManager::CreateScheduler() {
  MutexLock lock(s_scheduler_mutex_);
  if (s_scheduler_ == nullptr) {
    s_scheduler_ = new scheduler::Scheduler();
    // reset count
    s_scheduler_ref_count_ = 0;
  }
  s_scheduler_ref_count_++;
}

void UserSecureManager::DestroyScheduler() {
  MutexLock lock(s_scheduler_mutex_);
  if (s_scheduler_ == nullptr) {
    // reset count
    s_scheduler_ref_count_ = 0;
    return;
  }
  s_scheduler_ref_count_--;
  if (s_scheduler_ref_count_ <= 0) {
    delete s_scheduler_;
    s_scheduler_ = nullptr;
    // reset count
    s_scheduler_ref_count_ = 0;
  }
}

}  // namespace secure
}  // namespace auth
}  // namespace firebase
