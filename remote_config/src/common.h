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

#ifndef FIREBASE_REMOTE_CONFIG_SRC_COMMON_H_
#define FIREBASE_REMOTE_CONFIG_SRC_COMMON_H_

#include <cstddef>

namespace firebase {
namespace remote_config {

// Internal error codes used when completing Remote Config Futures.
enum FutureStatus {
  kFutureStatusSuccess = 0,
  kFutureStatusFailure = 1,
};

namespace internal {

using ::firebase::remote_config::FutureStatus;
using ::firebase::remote_config::kFutureStatusFailure;
using ::firebase::remote_config::kFutureStatusSuccess;

enum RemoteConfigFn {
  kRemoteConfigFnFetch,
  kRemoteConfigFnEnsureInitialized,
  kRemoteConfigFnActivate,
  kRemoteConfigFnFetchAndActivate,
  kRemoteConfigFnSetDefaults,
  kRemoteConfigFnSetConfigSettings,
  kRemoteConfigFnSetCustomSignals,
  kRemoteConfigFnCount,
};

// Maximum number of custom signals allowed.
constexpr size_t kMaxCustomSignalsCount = 100;
// Maximum length of a custom signal key.
constexpr size_t kMaxCustomSignalKeyLength = 256;
// Maximum length of a custom signal string value.
constexpr size_t kMaxCustomSignalValueLength = 500;

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_COMMON_H_
