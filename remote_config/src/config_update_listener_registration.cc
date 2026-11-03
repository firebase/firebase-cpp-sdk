// Copyright 2023 Google LLC
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

#include "remote_config/src/include/firebase/remote_config/config_update_listener_registration.h"

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/platform.h"
#include "remote_config/src/config_update_listener_registration_internal.h"

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

void ConfigUpdateListenerRegistration::CleanupConfigUpdateListenerRegistration(
    void* registration) {
  if (!registration) return;
  ConfigUpdateListenerRegistration* listener_registration =
      reinterpret_cast<ConfigUpdateListenerRegistration*>(registration);
  listener_registration->internal_ = nullptr;
  listener_registration->remote_config_ = nullptr;
}

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration() = default;

ConfigUpdateListenerRegistration::~ConfigUpdateListenerRegistration() {
  if (remote_config_) {
    remote_config_->cleanup_notifier().UnregisterObject(this);
  }
  internal_ = nullptr;
  remote_config_ = nullptr;
}

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration(
    const ConfigUpdateListenerRegistration& other)
    : internal_(other.internal_), remote_config_(other.remote_config_) {
  if (remote_config_) {
    remote_config_->cleanup_notifier().RegisterObject(
        this, CleanupConfigUpdateListenerRegistration);
  }
}

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration(
    ConfigUpdateListenerRegistration&& other) noexcept
    : internal_(other.internal_), remote_config_(other.remote_config_) {
  if (other.remote_config_) {
    other.remote_config_->cleanup_notifier().UnregisterObject(&other);
  }
  other.internal_ = nullptr;
  other.remote_config_ = nullptr;
  if (remote_config_) {
    remote_config_->cleanup_notifier().RegisterObject(
        this, CleanupConfigUpdateListenerRegistration);
  }
}

ConfigUpdateListenerRegistration& ConfigUpdateListenerRegistration::operator=(
    const ConfigUpdateListenerRegistration& other) {
  if (this == &other) {
    return *this;
  }
  if (remote_config_) {
    remote_config_->cleanup_notifier().UnregisterObject(this);
  }
  internal_ = other.internal_;
  remote_config_ = other.remote_config_;
  if (remote_config_) {
    remote_config_->cleanup_notifier().RegisterObject(
        this, CleanupConfigUpdateListenerRegistration);
  }
  return *this;
}

ConfigUpdateListenerRegistration& ConfigUpdateListenerRegistration::operator=(
    ConfigUpdateListenerRegistration&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (remote_config_) {
    remote_config_->cleanup_notifier().UnregisterObject(this);
  }
  if (other.remote_config_) {
    other.remote_config_->cleanup_notifier().UnregisterObject(&other);
  }
  internal_ = other.internal_;
  remote_config_ = other.remote_config_;
  other.internal_ = nullptr;
  other.remote_config_ = nullptr;
  if (remote_config_) {
    remote_config_->cleanup_notifier().RegisterObject(
        this, CleanupConfigUpdateListenerRegistration);
  }
  return *this;
}

ConfigUpdateListenerRegistration::ConfigUpdateListenerRegistration(
    internal::ConfigUpdateListenerRegistrationInternal* internal) {
  if (internal != nullptr && internal->remote_config() != nullptr) {
    internal_ = internal;
    remote_config_ = internal->remote_config();
    remote_config_->cleanup_notifier().RegisterObject(
        this, CleanupConfigUpdateListenerRegistration);
  }
}

void ConfigUpdateListenerRegistration::Remove() {
  if (internal_ != nullptr) {
    internal_->Remove();
  }
}

}  // namespace remote_config
}  // namespace firebase
