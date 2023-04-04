// Copyright 2016 Google LLC
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

#ifndef FIREBASE_REMOTE_CONFIG_SRC_COMMON_H_
#define FIREBASE_REMOTE_CONFIG_SRC_COMMON_H_

#include "app/src/reference_counted_future_impl.h"
#include "app/src/semaphore.h"

namespace firebase {
namespace remote_config {

enum RemoteConfigFn {
  kRemoteConfigFnFetch,
  kRemoteConfigFnEnsureInitialized,
  kRemoteConfigFnActivate,
  kRemoteConfigFnFetchAndActivate,
  kRemoteConfigFnSetDefaults,
  kRemoteConfigFnSetConfigSettings,
  kRemoteConfigFnCount
};

/// @brief Describes the error codes returned by futures.
enum FutureStatus {
  // The future returned successfully.
  // This should always evaluate to zero, to ensure that the future returns
  // a zero result on success.
  kFutureStatusSuccess = 0,
  // The future returned unsuccessfully.  Check GetInfo() for further details.
  kFutureStatusFailure,
};

// Data structure which holds the Future API implementation with the only
// future required by this API (fetch_future_).
class FutureData {
 public:
  FutureData() : api_(kRemoteConfigFnCount) {}
  ~FutureData() {}

  ReferenceCountedFutureImpl* api() { return &api_; }

  // Create FutureData singleton.
  static FutureData* Create();
  // Destroy the FutureData singleton.
  static void Destroy();
  // Get the Future data singleton.
  static FutureData* Get();

 private:
  ReferenceCountedFutureImpl api_;

  static FutureData* s_future_data_;
};

namespace internal {

// Determines whether remote config is initialized.
// Implemented in each platform module.
bool IsInitialized();

// Waits until the given future is complete and asserts that it completed with
// the given error (no error by default). Returns the future's result.
template <typename T>
void WaitForFuture(const firebase::Future<T>& future, Semaphore* future_sem,
                   const char* action_name) {
  // Block and wait until Future is complete.
  future.OnCompletion(
      [](const firebase::Future<T>& result, void* data) {
        Semaphore* sem = static_cast<Semaphore*>(data);
        sem->Post();
      },
      future_sem);
  future_sem->Wait();

  if (future.status() == firebase::kFutureStatusComplete &&
      future.error() == kFutureStatusSuccess) {
    LogDebug("RemoteConfig Future: %s Success", action_name);
  } else if (future.status() != firebase::kFutureStatusComplete) {
    // It is fine if timeout
    LogWarning("RemoteConfig Future: %s timeout", action_name);
  } else {
    // It is fine if failed
    LogWarning("RemoteConfig Future: Failed to %s. Error %d: %s", action_name,
               future.error(), future.error_message());
  }
}

}  // namespace internal

}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_COMMON_H_
