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

#include "remote_config/src/include/firebase/remote_config.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "app/src/app_state.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/time.h"
#include "remote_config/src/common.h"

// RemoteConfigInternal is defined in these 3 files, one implementation for each
// OS.
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

namespace {
constexpr int kRemoteConfigServiceTag = 0;
}  // namespace

RemoteConfig::RemoteConfig(
    std::shared_ptr<internal::RemoteConfigInternal> internal)
    : internal_(std::move(internal)) {}

RemoteConfig::~RemoteConfig() = default;

App RemoteConfig::app() const {
  return internal_ ? internal_->app() : App();
}

RemoteConfig RemoteConfig::GetInstance(const App& app) {
  if (!app.is_valid()) return {};

  const auto& state = ::firebase::internal::AppState::FromApp(app);
  if (!state) return {};

  auto internal = state->GetOrCreateService<internal::RemoteConfigInternal>(
      &kRemoteConfigServiceTag, {},
      [](::firebase::internal::AppState& app_state)
          -> std::shared_ptr<internal::RemoteConfigInternal> {
        LogDebug("Creating RemoteConfig for App %s", app_state.name().c_str());
        auto instance =
            std::make_shared<internal::RemoteConfigInternal>(app_state);
        if (!instance->Initialized()) {
          return {};
        }
        return instance;
      });
  if (!internal) return {};
  return RemoteConfig(std::move(internal));
}

Future<ConfigInfo> RemoteConfig::EnsureInitialized() {
  if (!internal_) return {};
  return internal_->EnsureInitialized();
}

Future<ConfigInfo> RemoteConfig::EnsureInitializedLastResult() {
  if (!internal_) return {};
  return internal_->EnsureInitializedLastResult();
}

Future<bool> RemoteConfig::Activate() {
  if (!internal_) return {};
  return internal_->Activate();
}

Future<bool> RemoteConfig::ActivateLastResult() {
  if (!internal_) return {};
  return internal_->ActivateLastResult();
}

Future<bool> RemoteConfig::FetchAndActivate() {
  if (!internal_) return {};
  return internal_->FetchAndActivate();
}

Future<bool> RemoteConfig::FetchAndActivateLastResult() {
  if (!internal_) return {};
  return internal_->FetchAndActivateLastResult();
}

Future<void> RemoteConfig::Fetch() {
  if (!internal_) return {};
  return Fetch(GetConfigSettings().minimum_fetch_interval_in_milliseconds /
               firebase::internal::kMillisecondsPerSecond);
}

Future<void> RemoteConfig::Fetch(uint64_t cache_expiration_in_seconds) {
  if (!internal_) return {};
  return internal_->Fetch(cache_expiration_in_seconds);
}

Future<void> RemoteConfig::FetchLastResult() {
  if (!internal_) return {};
  return internal_->FetchLastResult();
}

#if defined(__ANDROID__)
Future<void> RemoteConfig::SetDefaults(int defaults_resource_id) {
  if (!internal_) return {};
  return internal_->SetDefaults(defaults_resource_id);
}
#endif  // __ANDROID__

Future<void> RemoteConfig::SetDefaults(const ConfigKeyValueVariant* defaults,
                                       size_t number_of_defaults) {
  if (!internal_) return {};
  return internal_->SetDefaults(defaults, number_of_defaults);
}

Future<void> RemoteConfig::SetDefaults(const ConfigKeyValue* defaults,
                                       size_t number_of_defaults) {
  if (!internal_) return {};
  return internal_->SetDefaults(defaults, number_of_defaults);
}

Future<void> RemoteConfig::SetDefaultsLastResult() {
  if (!internal_) return {};
  return internal_->SetDefaultsLastResult();
}

Future<void> RemoteConfig::SetConfigSettings(ConfigSettings settings) {
  if (!internal_) return {};
  return internal_->SetConfigSettings(settings);
}

ConfigSettings RemoteConfig::GetConfigSettings() {
  if (!internal_) return {};
  return internal_->GetConfigSettings();
}

Future<void> RemoteConfig::SetConfigSettingsLastResult() {
  if (!internal_) return {};
  return internal_->SetConfigSettingsLastResult();
}

Future<void> RemoteConfig::SetCustomSignals(
    const std::map<std::string, Variant>& custom_signals) {
  if (!internal_) return {};
  return internal_->SetCustomSignals(custom_signals);
}

Future<void> RemoteConfig::SetCustomSignalsLastResult() {
  if (!internal_) return {};
  return internal_->SetCustomSignalsLastResult();
}

bool RemoteConfig::GetBoolean(const char* key) {
  return GetBoolean(key, static_cast<ValueInfo*>(nullptr));
}

bool RemoteConfig::GetBoolean(const char* key, ValueInfo* info) {
  if (!internal_) return false;
  return internal_->GetBoolean(key, info);
}

int64_t RemoteConfig::GetLong(const char* key) {
  return GetLong(key, static_cast<ValueInfo*>(nullptr));
}

int64_t RemoteConfig::GetLong(const char* key, ValueInfo* info) {
  if (!internal_) return 0;
  return internal_->GetLong(key, info);
}

double RemoteConfig::GetDouble(const char* key) {
  return GetDouble(key, static_cast<ValueInfo*>(nullptr));
}

double RemoteConfig::GetDouble(const char* key, ValueInfo* info) {
  if (!internal_) return 0.0;
  return internal_->GetDouble(key, info);
}

std::string RemoteConfig::GetString(const char* key) {
  return GetString(key, static_cast<ValueInfo*>(nullptr));
}

std::string RemoteConfig::GetString(const char* key, ValueInfo* info) {
  if (!internal_) return {};
  return internal_->GetString(key, info);
}

std::vector<unsigned char> RemoteConfig::GetData(const char* key) {
  return GetData(key, static_cast<ValueInfo*>(nullptr));
}

std::vector<unsigned char> RemoteConfig::GetData(const char* key,
                                                 ValueInfo* info) {
  if (!internal_) return {};
  return internal_->GetData(key, info);
}

std::vector<std::string> RemoteConfig::GetKeysByPrefix(const char* prefix) {
  if (!internal_) return {};
  return internal_->GetKeysByPrefix(prefix);
}

std::vector<std::string> RemoteConfig::GetKeys() {
  if (!internal_) return {};
  return internal_->GetKeys();
}

std::map<std::string, Variant> RemoteConfig::GetAll() {
  if (!internal_) return {};
  return internal_->GetAll();
}

// TODO(b/147143718): Change to a more descriptive name.
const ConfigInfo RemoteConfig::GetInfo() {
  if (!internal_) return {};
  return internal_->GetInfo();
}

ConfigUpdateListenerRegistration RemoteConfig::AddOnConfigUpdateListener(
    std::function<void(ConfigUpdate&&, RemoteConfigError)>
        config_update_listener) {
  if (!internal_) return {};
  return internal_->AddOnConfigUpdateListener(config_update_listener);
}

}  // namespace remote_config
}  // namespace firebase
