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

#ifndef FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_COMMON_H_
#define FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_COMMON_H_

#include "app/src/reference_counted_future_impl.h"

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

  static FutureData *s_future_data_;
};

namespace internal {

// Determines whether remote config is initialized.
// Implemented in each platform module.
bool IsInitialized();

// Registers a cleanup task for this module if auto-initialization is disabled.
void RegisterTerminateOnDefaultAppDestroy();

// Remove the cleanup task for this module if auto-initialization is disabled.
void UnregisterTerminateOnDefaultAppDestroy();

}  // namespace internal

}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_COMMON_H_
