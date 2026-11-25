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

#include "analytics/src/analytics_common.h"

#include <memory>
#include <string_view>

#include "analytics/src/include/firebase/analytics.h"
#include "app/src/app_state.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/no_destructor.h"
#include "app/src/util.h"
// Include the generated headers to force their compilation for test purposes.
#include "analytics/src/include/firebase/analytics/event_names.h"
#include "analytics/src/include/firebase/analytics/parameter_names.h"
#include "analytics/src/include/firebase/analytics/user_property_names.h"

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    analytics,
    {
      if (std::string_view(app.name()) == kDefaultAppName) {
        firebase::analytics::Initialize(app);
      }
      return kInitResultSuccess;
    },
    false);

namespace firebase {
namespace analytics {
namespace internal {

namespace {

constexpr int kAnalyticsTeardownTag = 0;

struct TeardownGuardState {
  Mutex mutex;
  std::weak_ptr<::firebase::internal::AppState> app_state;
  std::weak_ptr<::firebase::internal::TeardownGuardService> guard;
};

TeardownGuardState& GetTeardownGuardState() {
  static ::firebase::internal::NoDestructor<TeardownGuardState> state;
  return *state;
}

}  // namespace

const char* kAnalyticsModuleName = "analytics";

::firebase::internal::ModuleLifecycleGate& GetLifecycleGate() {
  static ::firebase::internal::NoDestructor<
      ::firebase::internal::ModuleLifecycleGate>
      gate;
  return *gate;
}

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

void RegisterTerminateOnAppDestroy(const App& app) {
  const auto& app_state = ::firebase::internal::AppState::FromApp(app);
  if (!app_state) return;

  UnregisterTerminateOnAppDestroy();

  const bool auto_init = AppCallback::GetEnabledByName(kAnalyticsModuleName);
  auto [owner, guard] = app_state->GetOrCreateDetachableService<
      ::firebase::internal::TeardownGuardService>(
      &kAnalyticsTeardownTag, {}, [auto_init](::firebase::internal::AppState&) {
        return std::make_shared<::firebase::internal::TeardownGuardService>(
            [auto_init]() {
              if (!auto_init) {
                LogError(
                    "analytics::Terminate() should be called before default "
                    "app is destroyed.");
              }
              if (firebase::analytics::internal::IsInitialized()) {
                firebase::analytics::Terminate();
              }
            });
      });
  if (!guard) return;

  auto& state = GetTeardownGuardState();
  MutexLock lock(state.mutex);
  state.app_state = owner;
  state.guard = guard;
}

void UnregisterTerminateOnAppDestroy() {
  std::weak_ptr<::firebase::internal::AppState> weak_state;
  std::weak_ptr<::firebase::internal::TeardownGuardService> weak_guard;
  {
    auto& state = GetTeardownGuardState();
    MutexLock lock(state.mutex);
    weak_state = std::move(state.app_state);
    weak_guard = std::move(state.guard);
  }
  auto guard = weak_guard.lock();
  if (guard) {
    guard->Cancel();
  }
  if (auto app_state = weak_state.lock()) {
    app_state->DetachService(&kAnalyticsTeardownTag, {}, guard.get());
  }
}

}  // namespace internal
}  // namespace analytics
}  // namespace firebase
