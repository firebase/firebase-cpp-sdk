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

#include "remote_config/src/common.h"

#include <assert.h>

#include "app/src/cleanup_notifier.h"
#include "app/src/util.h"
#include "remote_config/src/include/firebase/remote_config.h"

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(remote_config,
                                {
                                  if (app == ::firebase::App::GetInstance()) {
                                    return firebase::remote_config::Initialize(
                                        *app);
                                  }
                                  return kInitResultSuccess;
                                },
                                {
                                  if (app == ::firebase::App::GetInstance()) {
                                    firebase::remote_config::Terminate();
                                  }
                                });

namespace firebase {
namespace remote_config {

namespace internal {

const char kRemoteConfigModuleName[] = "remote_config";

// Registers a cleanup task for this module if auto-initialization is disabled.
void RegisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kRemoteConfigModuleName)) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(App::GetInstance());
    assert(cleanup_notifier);
    cleanup_notifier->RegisterObject(
        const_cast<char*>(kRemoteConfigModuleName), [](void*) {
          LogError(
              "remote_config::Terminate() should be called before default app "
              "is destroyed.");
          if (firebase::remote_config::internal::IsInitialized()) {
            firebase::remote_config::Terminate();
          }
        });
  }
}

// Remove the cleanup task for this module if auto-initialization is disabled.
void UnregisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kRemoteConfigModuleName) &&
      firebase::remote_config::internal::IsInitialized()) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(App::GetInstance());
    assert(cleanup_notifier);
    cleanup_notifier->UnregisterObject(
        const_cast<char*>(kRemoteConfigModuleName));
  }
}

}  // namespace internal

FutureData* FutureData::s_future_data_;

// Create FutureData singleton.
FutureData* FutureData::Create() {
  s_future_data_ = new FutureData();
  return s_future_data_;
}

// Destroy the FutureData singleton.
void FutureData::Destroy() {
  delete s_future_data_;
  s_future_data_ = nullptr;
}

// Get the Future data singleton.
FutureData* FutureData::Get() { return s_future_data_; }

}  // namespace remote_config
}  // namespace firebase
