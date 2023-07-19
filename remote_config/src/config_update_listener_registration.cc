/*
 * Copyright 2023 Google LLC
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

#include "remote_config/src/include/firebase/remote_config/config_update_listener_registration.h"

#include <utility>

#include "app/src/include/firebase/internal/platform.h"
#include "remote_config/src/cleanup.h"
#include "remote_config/src/config_update_listener_registration_internal.h"

// The Platform-specific implementation of RemoteConfigInternal is needed
// so that CleanupFn can reference the cleanup_notifier
#if FIREBASE_PLATFORM_ANDROID
#include "remote_config/src/android/remote_config_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "remote_config/src/ios/remote_config_ios.h"
#else
#include "remote_config/src/desktop/remote_config_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase {
namespace remote_config {

// ConfigUpdateListenerRegistration specific fact:
// ConfigUpdateListenerRegistration does NOT own the
// ConfigUpdateListenerRegistrationInternal object, which is different from
// other wrapper types. RemoteConfigInternal owns all
// ConfigUpdateListenerRegistrationInternal objects instead. So
// RemoteConfigInternal can remove all listeners upon destruction.

using CleanupFnConfigUpdateListenerRegistration =
    CleanupFn<ConfigUpdateListenerRegistration, internal::RemoteConfigInternal>;

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration()
    : remote_config_(nullptr), internal_(nullptr) {
  // Default constructor. Creates a no-op registration, no cleanup needed.
}

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration(
    const ConfigUpdateListenerRegistration& registration)
    : remote_config_(registration.remote_config_),
      internal_(registration.internal_) {
  // Copy constructor. Register cleanup for the newly created object.
  CleanupFnConfigUpdateListenerRegistration::Register(this, remote_config_);
}

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration(
    ConfigUpdateListenerRegistration&& registration)
    : remote_config_(registration.remote_config_) {
  // Move constructor. Unregister cleanup for the old object and transfer
  // ownership of the internal data.
  CleanupFnConfigUpdateListenerRegistration::Unregister(
      &registration, registration.remote_config_);
  std::swap(internal_, registration.internal_);
  CleanupFnConfigUpdateListenerRegistration::Register(this, remote_config_);
}

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration(
    internal::ConfigUpdateListenerRegistrationInternal* internal)
    : remote_config_(internal == nullptr ? nullptr
                                         : internal->remote_config_internal()),
      internal_(internal) {
  // Normal constructor. Register cleanup for the newly created registration.
  CleanupFnConfigUpdateListenerRegistration::Register(this, remote_config_);
}

ConfigUpdateListenerRegistration::~ConfigUpdateListenerRegistration() {
  // Destructor. Unregister cleanup as it is no longer needed.
  CleanupFnConfigUpdateListenerRegistration::Unregister(this, remote_config_);
  internal_ = nullptr;
}

ConfigUpdateListenerRegistration& ConfigUpdateListenerRegistration::operator=(
    const ConfigUpdateListenerRegistration& registration) {
  if (this == &registration) {
    return *this;
  }

  // Copy assignment operator. Unregister cleanup for this object before
  // changing the value of remote_config_ in case it was previously owned
  // by a different instance of remote config.
  CleanupFnConfigUpdateListenerRegistration::Unregister(this, remote_config_);
  remote_config_ = registration.remote_config_;
  internal_ = registration.internal_;
  CleanupFnConfigUpdateListenerRegistration::Register(this, remote_config_);
  return *this;
}

ConfigUpdateListenerRegistration& ConfigUpdateListenerRegistration::operator=(
    ConfigUpdateListenerRegistration&& registration) {
  if (this == &registration) {
    return *this;
  }

  // Move assignment operator.
  // Unregister cleanup for the object that we are moving from.
  CleanupFnConfigUpdateListenerRegistration::Unregister(
      &registration, registration.remote_config_);
  // Unregister cleanup for this object before changing the value of
  // remote_config_ in case it was previously owned by a different instance
  // of remote config.
  CleanupFnConfigUpdateListenerRegistration::Unregister(this, remote_config_);
  remote_config_ = registration.remote_config_;
  internal_ = registration.internal_;
  CleanupFnConfigUpdateListenerRegistration::Register(this, remote_config_);
  return *this;
}

void ConfigUpdateListenerRegistration::Remove() {
  // The check for remote_config_ is required. User can hold a
  // ListenerRegistration instance indefinitely even after RemoteConfig is
  // destructed, in which case remote_config_ will be reset to nullptr by the
  // Cleanup function. The check for internal_ is optional. Actually internal_
  // can be of following cases:
  //   * nullptr if already called Remove() on the same instance;
  //   * not nullptr but invalid if Remove() is called on a copy of this;
  //   * not nullptr and valid.
  // Unregister a nullptr or invalid ListenerRegistration is a no-op.
  if (internal_ && remote_config_) {
    internal_->Remove();
    internal_ = nullptr;
  }
}

// This cleanup method is called by RemoteConfig's CleanupNotifier when
// RemoteConfigInternal is being destroyed.
void ConfigUpdateListenerRegistration::Cleanup() {
  // Just clear references to internal types. No need to call Remove here
  // since RemoteConfigInternal will cleanup the internal objects separately.
  remote_config_ = nullptr;
  internal_ = nullptr;
}

}  // namespace remote_config
}  // namespace firebase
