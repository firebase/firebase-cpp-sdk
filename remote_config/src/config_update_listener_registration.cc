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

#include "remote_config/src/cleanup.h"
#include "remote_config/src/config_update_listener_registration_internal.h"

namespace firebase {

namespace remote_config {
// ConfigUpdateListenerRegistration specific fact:
//   ConfigUpdateListenerRegistration does NOT own the
//   ConfigUpdateListenerRegistrationInternal object, which is different from
//   other wrapper types. RemoteConfigInternal owns all
//   ConfigUpdateListenerRegistrationInternal objects instead. So
//   RemoteConfigInternal can remove all listeners upon destruction.

using CleanupFnConfigUpdateListenerRegistration = CleanupFn<ConfigUpdateListenerRegistration, RemoteConfigInternal>;

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration(
    const ConfigUpdateListenerRegistration& registration)
    : remote_config_(registration.remote_config_) {
  internal_ = registration.internal_;
  CleanupFnConfigUpdateListenerRegistration::Register(this, remote_config_);
}

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration(ConfigUpdateListenerRegistration&& registration)
    : remote_config_(registration.remote_config_) {
  CleanupFnConfigUpdateListenerRegistration::Unregister(&registration,
                                            registration.remote_config_);
  std::swap(internal_, registration.internal_);
  CleanupFnConfigUpdateListenerRegistration::Register(this, remote_config_);
}

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration(
    ConfigUpdateListenerRegistrationInternal* internal)
    : remote_config_(internal == nullptr ? nullptr
                                     : internal->remote_config_internal()),
      internal_(internal) {
  CleanupFnConfigUpdateListenerRegistration::Register(this, remote_config_);
}

ConfigUpdateListenerRegistration::~ConfigUpdateListenerRegistration() {
  CleanupFnConfigUpdateListenerRegistration::Unregister(this, remote_config_);
  internal_ = nullptr;
}

ConfigUpdateListenerRegistration& ConfigUpdateListenerRegistration::operator=(
    const ConfigUpdateListenerRegistration& registration) {
  if (this == &registration) {
    return *this;
  }

  remote_config_ = registration.remote_config_;
  CleanupFnConfigUpdateListenerRegistration::Unregister(this, remote_config_);
  internal_ = registration.internal_;
  CleanupFnConfigUpdateListenerRegistration::Register(this, remote_config_);
  return *this;
}

ConfigUpdateListenerRegistration& ConfigUpdateListenerRegistration::operator=(
    ConfigUpdateListenerRegistration&& registration) {
  if (this == &registration) {
    return *this;
  }

  remote_config_ = registration.remote_config_;
  CleanupFnConfigUpdateListenerRegistration::Unregister(&registration,
                                            registration.remote_config_);
  CleanupFnConfigUpdateListenerRegistration::Unregister(this, remote_config_);
  internal_ = registration.internal_;
  CleanupFnConfigUpdateListenerRegistration::Register(this, remote_config_);
  return *this;
}

void ConfigUpdateListenerRegistration::Remove() {
  // The check for remote_config_ is required. User can hold a ListenerRegistration
  // instance indefinitely even after RemoteConfig is destructed, in which case
  // remote_config_ will be reset to nullptr by the Cleanup function.
  // The check for internal_ is optional. Actually internal_ can be of following
  // cases:
  //   * nullptr if already called Remove() on the same instance;
  //   * not nullptr but invalid if Remove() is called on a copy of this;
  //   * not nullptr and valid.
  // Unregister a nullptr or invalid ListenerRegistration is a no-op.
  if (internal_ && remote_config_) {
    internal_->Remove();
    // TODO(almostmatt): maybe unregister hte cleanup fn?
    // though theoretically it is fine to stay registered for now
    // remote_config_->UnregisterListenerRegistration(internal_);
    // this might be comparable to just calling the destructor on internal.
    // Note: would like to call each destructor once. I think that is what
    // is accomplished by
    // firestore->UnregisterListenerRegistration(internal_);
    // a simpler version might just register and delete on delete.
    internal_ = nullptr;
  }
}

void ConfigUpdateListenerRegistration::Cleanup() {
  Remove();
  remote_config_ = nullptr;
}

}  // namespace remote_config
}  // namespace firebase
