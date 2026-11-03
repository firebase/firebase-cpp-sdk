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

#include "app/src/secure/user_secure_manager.h"

#include <cctype>
#include <cstdint>
#include <utility>

#include "app/src/base64.h"
#include "app/src/callback.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/secure/user_secure_internal.h"

// Only build this implementation on desktop. If building for iOS or Android,
// omit it entirely. Until we implement UserSecureInternal for those platforms,
// referencing this class will cause a linker error.
// TODO(b/132622988): Add mobile support.

#if FIREBASE_PLATFORM_DESKTOP

#if defined(_WIN32)
#include "app/src/secure/user_secure_windows_internal.h"
#define USER_SECURE_TYPE UserSecureWindowsInternal

#elif defined(TARGET_OS_OSX) && TARGET_OS_OSX
#include "app/src/secure/user_secure_darwin_internal.h"
#define USER_SECURE_TYPE UserSecureDarwinInternal

#elif defined(__linux__)
#include "app/src/secure/user_secure_linux_internal.h"
#define USER_SECURE_TYPE UserSecureLinuxInternal

#else  // Unknown platform, use fake version.
#warning "No secure storage is available on this platform."
#include "app/src/secure/user_secure_fake_internal.h"
#define USER_SECURE_TYPE UserSecureFakeInternal
#endif

namespace firebase {
namespace app {
namespace secure {

using callback::NewCallback;

Mutex* UserSecureManager::s_scheduler_mutex_ = new Mutex();
scheduler::Scheduler* UserSecureManager::s_scheduler_;
int32_t UserSecureManager::s_scheduler_ref_count_;

UserSecureManager::UserSecureManager(const char* domain, const char* app_id)
    : future_api_(kUserSecureFnCount), safe_this_(this) {
  user_secure_ = std::make_unique<USER_SECURE_TYPE>(domain, app_id);
  CreateScheduler();
}

UserSecureManager::UserSecureManager(
    std::unique_ptr<UserSecureInternal> user_secure_internal)
    : user_secure_(std::move(user_secure_internal)),
      future_api_(kUserSecureFnCount),
      safe_this_(this) {
  CreateScheduler();
}

UserSecureManager::~UserSecureManager() {
  // Clear safe reference first so any already-dequeued callback that has not
  // yet acquired ThisRefLock will observe nullptr and allow
  // CancelScheduledTasks() to complete its Future with kErrorCancelled.
  safe_this_.ClearReference();
  CancelScheduledTasks();
  DestroyScheduler();
}

Future<std::string> UserSecureManager::LoadUserData(
    const std::string& app_name) {
  const auto future_handle =
      future_api_.SafeAlloc<std::string>(kUserSecureFnLoad);

  auto data_handle = std::make_shared<UserSecureDataHandle<std::string>>(
      app_name, "", &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, std::shared_ptr<UserSecureDataHandle<std::string>> handle,
         UserSecureInternal* internal) {
        FIREBASE_ASSERT(internal);
        {
          ThisRefLock lock(&ref);
          UserSecureManager* manager = lock.GetReference();
          if (manager == nullptr) {
            return;
          }
          std::string result = internal->LoadUserData(handle->app_name);
          manager->CompleteOperationRegistration(
              kLoadUserData, handle->future_handle.get().id());
          auto* api = handle->future_api;
          firebase::FutureHandle raw_handle = handle->future_handle.get();
          handle->future_handle = firebase::SafeFutureHandle<std::string>();
          handle->future_api = nullptr;
          std::string empty_str("");
          if (result.empty()) {
            std::string message(
                "Failed to read user data for app (" + handle->app_name +
                ").  This could happen if the current user doesn't have access "
                "to the keystore, the keystore has been corrupted or the app "
                "intentionally deleted the stored data.");
            api->CompleteWithResult(raw_handle, kNoEntry, message.c_str(), empty_str);
          } else {
            api->CompleteWithResult(raw_handle, kSuccess, "", result);
          }
        }
      },
      safe_this_, data_handle, user_secure_.get());

  ScheduleOperation(kLoadUserData, future_handle, callback);
  return MakeFuture(&future_api_, future_handle);
}

Future<void> UserSecureManager::SaveUserData(const std::string& app_name,
                                             const std::string& user_data) {
  const auto future_handle = future_api_.SafeAlloc<void>(kUserSecureFnSave);

  auto data_handle = std::make_shared<UserSecureDataHandle<void>>(
      app_name, user_data, &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, std::shared_ptr<UserSecureDataHandle<void>> handle,
         UserSecureInternal* internal) {
        FIREBASE_ASSERT(internal);
        {
          ThisRefLock lock(&ref);
          UserSecureManager* manager = lock.GetReference();
          if (manager == nullptr) {
            return;
          }
          internal->SaveUserData(handle->app_name, handle->user_data);
          manager->CompleteOperationRegistration(
              kSaveUserData, handle->future_handle.get().id());
          handle->future_api->Complete(handle->future_handle, kSuccess);
        }
      },
      safe_this_, data_handle, user_secure_.get());

  ScheduleOperation(kSaveUserData, future_handle, callback);
  return MakeFuture(&future_api_, future_handle);
}

Future<void> UserSecureManager::DeleteUserData(const std::string& app_name) {
  const auto future_handle = future_api_.SafeAlloc<void>(kUserSecureFnDelete);

  auto data_handle = std::make_shared<UserSecureDataHandle<void>>(
      app_name, "", &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, std::shared_ptr<UserSecureDataHandle<void>> handle,
         UserSecureInternal* internal) {
        FIREBASE_ASSERT(internal);
        {
          ThisRefLock lock(&ref);
          UserSecureManager* manager = lock.GetReference();
          if (manager == nullptr) {
            return;
          }
          internal->DeleteUserData(handle->app_name);
          manager->CompleteOperationRegistration(
              kDeleteUserData, handle->future_handle.get().id());
          handle->future_api->Complete(handle->future_handle, kSuccess);
        }
      },
      safe_this_, data_handle, user_secure_.get());

  ScheduleOperation(kDeleteUserData, future_handle, callback);
  return MakeFuture(&future_api_, future_handle);
}

Future<void> UserSecureManager::DeleteAllData() {
  auto future_handle = future_api_.SafeAlloc<void>(kUserSecureFnDeleteAll);

  auto data_handle = std::make_shared<UserSecureDataHandle<void>>(
      "", "", &future_api_, future_handle);

  auto callback = NewCallback(
      [](ThisRef ref, std::shared_ptr<UserSecureDataHandle<void>> handle,
         UserSecureInternal* internal) {
        FIREBASE_ASSERT(internal);
        {
          ThisRefLock lock(&ref);
          UserSecureManager* manager = lock.GetReference();
          if (manager == nullptr) {
            return;
          }
          internal->DeleteAllData();
          manager->CompleteOperationRegistration(
              kDeleteAllData, handle->future_handle.get().id());
          handle->future_api->Complete(handle->future_handle, kSuccess);
        }
      },
      safe_this_, data_handle, user_secure_.get());

  ScheduleOperation(kDeleteAllData, future_handle, callback);
  return MakeFuture(&future_api_, future_handle);
}

void UserSecureManager::CreateScheduler() {
  MutexLock lock(*s_scheduler_mutex_);
  if (s_scheduler_ == nullptr) {
    s_scheduler_ = new scheduler::Scheduler();
    // reset count
    s_scheduler_ref_count_ = 0;
  }
  s_scheduler_ref_count_++;
}

void UserSecureManager::DestroyScheduler() {
  scheduler::Scheduler* scheduler_to_delete = nullptr;
  {
    MutexLock lock(*s_scheduler_mutex_);
    if (s_scheduler_ == nullptr) {
      // reset count
      s_scheduler_ref_count_ = 0;
      return;
    }
    s_scheduler_ref_count_--;
    if (s_scheduler_ref_count_ <= 0) {
      scheduler_to_delete = s_scheduler_;
      s_scheduler_ = nullptr;
      // reset count
      s_scheduler_ref_count_ = 0;
    }
  }
  delete scheduler_to_delete;
}

static bool IsHexDigit(char c) {
  if (c >= '0' && c <= '9') return true;
  if (c >= 'A' && c <= 'F') return true;
  return false;
}

static uint8_t HexToValue(char digit) {
  if (digit >= '0' && digit <= '9') {
    return static_cast<uint8_t>(digit - '0');
  }
  if (digit >= 'A' && digit <= 'F') {
    return 10 + static_cast<uint8_t>(digit - 'A');
  }
  FIREBASE_ASSERT("Not a hex digit" == nullptr);
  return 0;
}

// A single character at the start of the encoding specifies how it's encoded,
// in case we change to different formats in the future.
static const char kHeaderHexEncoded = '$';
static const char kHeaderBase64Encoded = '#';

bool UserSecureManager::AsciiToBinary(const std::string& encoded,
                                      std::string* decoded) {
  FIREBASE_ASSERT(decoded != nullptr);
  if (encoded.length() == 0) {
    // Should be at least one byte of header.
    *decoded = std::string();
    return false;
  }
  if (encoded[0] == kHeaderHexEncoded) {
    // Decode hex bytes.
    if (encoded.length() % 2 != 1) {
      *decoded = std::string();
      return false;
    }
    decoded->resize((encoded.length() - 1) / 2);
    for (size_t e = 1, d = 0; e < encoded.length(); ++d, e += 2) {
      char hi = static_cast<char>(
          std::toupper(static_cast<unsigned char>(encoded[e + 0])));
      char lo = static_cast<char>(
          std::toupper(static_cast<unsigned char>(encoded[e + 1])));
      if (!IsHexDigit(hi) || !IsHexDigit(lo)) {
        *decoded = std::string();
        return false;
      }
      (*decoded)[d] =
          static_cast<char>((HexToValue(hi) << 4) | HexToValue(lo));
    }
    return true;
  } else if (encoded[0] == kHeaderBase64Encoded) {
    return internal::Base64Decode(encoded.substr(1), decoded);
  } else {
    // Unknown header byte, can't decode.
    *decoded = std::string();
    return false;
  }
}

void UserSecureManager::BinaryToAscii(const std::string& original,
                                      std::string* encoded) {
  FIREBASE_ASSERT(encoded != nullptr);

  // Use base64 encoding.
  std::string base64;
  if (internal::Base64Encode(original, &base64)) {
    *encoded = std::string() + kHeaderBase64Encoded + base64;
  } else {
    *encoded = std::string();
  }
}

void UserSecureManager::CompleteOperationRegistration(
    SecureOperationType operation_type, FutureHandleId future_id) {
  MutexLock lock(operation_mutex_);
  operation_cancel_callbacks_.erase(future_id);
  auto id_it = operation_future_ids_.find(operation_type);
  if (id_it != operation_future_ids_.end() && id_it->second == future_id) {
    operation_future_ids_.erase(id_it);
    operation_handles_.erase(operation_type);
  }
}

void UserSecureManager::CancelScheduledTasks() {
  std::map<SecureOperationType, scheduler::RequestHandle> handles;
  std::map<FutureHandleId, std::function<void()>> cancel_callbacks;
  {
    MutexLock lock(operation_mutex_);
    handles = std::move(operation_handles_);
    operation_handles_.clear();
    cancel_callbacks = std::move(operation_cancel_callbacks_);
    operation_cancel_callbacks_.clear();
    operation_future_ids_.clear();
  }
  for (auto& entry : handles) {
    entry.second.Cancel();
  }
  for (auto& cancel_entry : cancel_callbacks) {
    if (cancel_entry.second) {
      cancel_entry.second();
    }
  }
}

void UserSecureManager::CancelOperation(SecureOperationType operation_type) {
  scheduler::RequestHandle handle_to_cancel;
  FutureHandleId future_id = 0;
  bool found = false;
  {
    MutexLock lock(operation_mutex_);
    auto it = operation_handles_.find(operation_type);
    if (it != operation_handles_.end()) {
      handle_to_cancel = it->second;
      operation_handles_.erase(it);
      found = true;
    }
    auto id_it = operation_future_ids_.find(operation_type);
    if (id_it != operation_future_ids_.end()) {
      future_id = id_it->second;
      operation_future_ids_.erase(id_it);
    }
  }
  if (found && handle_to_cancel.Cancel()) {
    std::function<void()> cancel_cb;
    {
      MutexLock lock(operation_mutex_);
      auto cancel_it = operation_cancel_callbacks_.find(future_id);
      if (cancel_it != operation_cancel_callbacks_.end()) {
        cancel_cb = std::move(cancel_it->second);
        operation_cancel_callbacks_.erase(cancel_it);
      }
    }
    if (cancel_cb) {
      cancel_cb();
    }
  }
}

template <typename T>
void UserSecureManager::ScheduleOperation(SecureOperationType operation_type,
                                          SafeFutureHandle<T> handle,
                                          callback::Callback* callback) {
  while (true) {
    CancelOperation(operation_type);
    MutexLock lock(operation_mutex_);
    if (operation_handles_.find(operation_type) != operation_handles_.end()) {
      continue;
    }
    const FutureHandleId future_id = handle.get().id();
    operation_future_ids_[operation_type] = future_id;
    operation_cancel_callbacks_[future_id] = [this, handle]() {
      if (future_api_.GetFutureStatus(handle.get()) == kFutureStatusPending) {
        future_api_.Complete(handle, kErrorCancelled,
                             "Operation was cancelled.");
      }
    };
    operation_handles_[operation_type] = s_scheduler_->Schedule(callback);
    break;
  }
}

}  // namespace secure
}  // namespace app
}  // namespace firebase

#endif  // FIREBASE_PLATFORM_DESKTOP
