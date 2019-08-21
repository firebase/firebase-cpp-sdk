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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app/src/app_common.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
#include "instance_id/src/include/firebase/instance_id.h"
#include "instance_id/src/instance_id_internal.h"

// Workaround MSVC's incompatible libc headers.
#if FIREBASE_PLATFORM_WINDOWS
#define snprintf _snprintf
#endif  // FIREBASE_PLATFORM_WINDOWS

// Module initializer does nothing at the moment.
FIREBASE_APP_REGISTER_CALLBACKS(instance_id,
                                { return firebase::kInitResultSuccess; }, {});

namespace firebase {
namespace instance_id {

namespace internal {

std::map<App*, InstanceId*>
    InstanceIdInternalBase::instance_id_by_app_;          // NOLINT
Mutex InstanceIdInternalBase::instance_id_by_app_mutex_;  // NOLINT

InstanceIdInternalBase::InstanceIdInternalBase()
    : future_api_(kApiFunctionMax) {
  static const char* kApiIdentifier = "InstanceId";
  future_api_id_.reserve(strlen(kApiIdentifier) +
                         16 /* hex characters in the pointer */ +
                         1 /* null terminator */);
  snprintf(&(future_api_id_[0]), future_api_id_.capacity(), "%s0x%016llx",
           kApiIdentifier,
           static_cast<unsigned long long>(  // NOLINT
               reinterpret_cast<intptr_t>(this)));
}

InstanceIdInternalBase::~InstanceIdInternalBase() {}

// Associate an InstanceId instance with an app.
void InstanceIdInternalBase::RegisterInstanceIdForApp(App* app,
                                                      InstanceId* instance_id) {
  MutexLock lock(instance_id_by_app_mutex_);
  instance_id_by_app_[app] = instance_id;

  // Cleanup this object if app is destroyed.
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
  assert(notifier);
  notifier->RegisterObject(instance_id, [](void* object) {
    InstanceId* instance_id_to_delete = reinterpret_cast<InstanceId*>(object);
    LogWarning(
        "InstanceId object 0x%08x should be deleted before the App 0x%08x it "
        "depends upon.",
        static_cast<int>(reinterpret_cast<intptr_t>(instance_id_to_delete)),
        static_cast<int>(
            reinterpret_cast<intptr_t>(&instance_id_to_delete->app())));
    instance_id_to_delete->DeleteInternal();
  });
  AppCallback::SetEnabledByName("instance_id", true);
}

// Remove association of InstanceId instance with an App.
void InstanceIdInternalBase::UnregisterInstanceIdForApp(
    App* app, InstanceId* instance_id) {
  MutexLock lock(instance_id_by_app_mutex_);

  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
  assert(notifier);
  notifier->UnregisterObject(instance_id);

  auto it = instance_id_by_app_.find(app);
  if (it == instance_id_by_app_.end()) return;
  assert(it->second == instance_id);
  (void)instance_id;
  instance_id_by_app_.erase(it);
}

// Find an InstanceId instance associated with an app.
InstanceId* InstanceIdInternalBase::FindInstanceIdByApp(App* app) {
  MutexLock lock(instance_id_by_app_mutex_);
  auto it = instance_id_by_app_.find(app);
  return it != instance_id_by_app_.end() ? it->second : nullptr;
}

}  // namespace internal

}  // namespace instance_id
}  // namespace firebase
