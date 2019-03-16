/*
 * Copyright 2016 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "analytics/src/include/firebase/analytics.h"

#include <assert.h>

#include "analytics/src/analytics_common.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/util.h"
// Include the generated headers to force their compilation for test purposes.
#include "analytics/src/include/firebase/analytics/event_names.h"
#include "analytics/src/include/firebase/analytics/parameter_names.h"
#include "analytics/src/include/firebase/analytics/user_property_names.h"

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(analytics,
                                {
                                  if (app == ::firebase::App::GetInstance()) {
                                    firebase::analytics::Initialize(*app);
                                  }
                                  return kInitResultSuccess;
                                },
                                {
                                  if (app == ::firebase::App::GetInstance()) {
                                    firebase::analytics::Terminate();
                                  }
                                });

namespace firebase {
namespace analytics {
namespace internal {

const char* kAnalyticsModuleName = "analytics";

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

// Registers a cleanup task for this module if auto-initialization is disabled.
void RegisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kAnalyticsModuleName)) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(App::GetInstance());
    assert(cleanup_notifier);
    cleanup_notifier->RegisterObject(
        const_cast<char*>(kAnalyticsModuleName), [](void*) {
          LogError(
              "analytics::Terminate() should be called before default app is "
              "destroyed.");
          if (firebase::analytics::internal::IsInitialized()) {
            firebase::analytics::Terminate();
          }
        });
  }
}

// Remove the cleanup task for this module if auto-initialization is disabled.
void UnregisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kAnalyticsModuleName) &&
      firebase::analytics::internal::IsInitialized()) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(App::GetInstance());
    assert(cleanup_notifier);
    cleanup_notifier->UnregisterObject(const_cast<char*>(kAnalyticsModuleName));
  }
}

}  // namespace internal
}  // namespace analytics
}  // namespace firebase
