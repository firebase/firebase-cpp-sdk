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

namespace firebase {
namespace auth {
namespace secure {

using callback::NewCallback;

UserSecureManager::UserSecureManager()
    : future_api_(kUserSecureFnCount), safe_this_(this) {
  // TODO(cynthiajiang) branch into different platforms (Linux, Mac, Windows)
  user_secure_ = MakeUnique<UserSecureLinuxInternal>();
}

UserSecureManager::UserSecureManager(
    UniquePtr<UserSecureInternal> userSecureInternal)
    : user_secure_(std::move(userSecureInternal)),
      future_api_(kUserSecureFnCount),
      safe_this_(this) {}

UserSecureManager::~UserSecureManager() {
  // Clear safe reference immediately so that scheduled callback can skip
  // executing code which requires reference to this.
  safe_this_.ClearReference();
}

Future<std::string> UserSecureManager::LoadUserData(
    const std::string& appName) {
  const auto future_handle =
      future_api_.SafeAlloc<std::string>(kUserSecureFnLoad);

  auto data_handle = MakeShared<UserSecureDataHandle<std::string>>(
      appName, "", &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, SharedPtr<UserSecureDataHandle<std::string>> handle,
         UserSecureInternal* internal) {
        ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          std::string result = internal->LoadUserData(handle->app_name);
          std::string empty_str("");
          if (result.empty()) {
            handle->future_api->CompleteWithResult(handle->future_handle,
                                                   kNoEntry, "", empty_str);
          } else {
            handle->future_api->CompleteWithResult(handle->future_handle,
                                                   kSuccess, "", result);
          }
        }
      },
      safe_this_, data_handle, user_secure_.get());
  scheduler_.Schedule(callback);
  return MakeFuture(&future_api_, future_handle);
}

Future<void> UserSecureManager::SaveUserData(const std::string& appName,
                                             const std::string& userData) {
  const auto future_handle = future_api_.SafeAlloc<void>(kUserSecureFnSave);

  auto data_handle = MakeShared<UserSecureDataHandle<void>>(
      appName, userData, &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, SharedPtr<UserSecureDataHandle<void>> handle,
         UserSecureInternal* internal) {
        ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          internal->SaveUserData(handle->app_name, handle->user_data);
          handle->future_api->Complete(handle->future_handle, kSuccess);
        }
      },
      safe_this_, data_handle, user_secure_.get());
  scheduler_.Schedule(callback);
  return MakeFuture(&future_api_, future_handle);
}

Future<void> UserSecureManager::DeleteUserData(const std::string& appName) {
  const auto future_handle = future_api_.SafeAlloc<void>(kUserSecureFnDelete);

  auto data_handle = MakeShared<UserSecureDataHandle<void>>(
      appName, "", &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, SharedPtr<UserSecureDataHandle<void>> handle,
         UserSecureInternal* internal) {
        ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          internal->DeleteUserData(handle->app_name);
          handle->future_api->Complete(handle->future_handle, kSuccess);
        }
      },
      safe_this_, data_handle, user_secure_.get());
  scheduler_.Schedule(callback);
  return MakeFuture(&future_api_, future_handle);
}

Future<void> UserSecureManager::DeleteAllData() {
  auto future_handle = future_api_.SafeAlloc<void>(kUserSecureFnDeleteAll);

  auto data_handle = MakeShared<UserSecureDataHandle<void>>(
      "", "", &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, SharedPtr<UserSecureDataHandle<void>> handle,
         UserSecureInternal* internal) {
        ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          internal->DeleteAllData();
          handle->future_api->Complete(handle->future_handle, kSuccess);
        }
      },
      safe_this_, data_handle, user_secure_.get());
  scheduler_.Schedule(callback);
  return MakeFuture(&future_api_, future_handle);
}

}  // namespace secure
}  // namespace auth
}  // namespace firebase
