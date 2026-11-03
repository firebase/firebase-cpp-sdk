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

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "remote_config/src/common.h"

// Querying the platform specific header files.
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

DEFINE_FIREBASE_VERSION_STRING(FirebaseRemoteConfig);

Mutex g_rc_lock;  // NOLINT
static std::map<App*, RemoteConfig*>* g_rc_instances = nullptr;

namespace {
std::map<App*, RemoteConfig*>& GetRcInstances() {
  if (!g_rc_instances) {
    g_rc_instances = new std::map<App*, RemoteConfig*>();
  }
  return *g_rc_instances;
}
}  // namespace

RemoteConfig* RemoteConfig::GetInstance(App* app) {
  if (!app) {
    LogError("RemoteConfig::GetInstance: App cannot be null.");
    return nullptr;
  }
  MutexLock lock(g_rc_lock);
  // Return the RemoteConfig if it already exists.
  RemoteConfig* existing_rc = FindRemoteConfig(app);
  if (existing_rc) {
    return existing_rc;
  }

  // Create a new RemoteConfig and initialize.
  RemoteConfig* rc = new RemoteConfig(app);
  LogDebug("Creating a new RemoteConfig instance for App %s", app->name());

  if (!rc->InitInternal()) {
    // Release the newly created RemoteConfig if internal initialization failed.
    LogError("Failed to initialize RemoteConfig instance for App %s",
             app->name());
    delete rc;
    return nullptr;
  }

  // Register the new RemoteConfig instance with CleanupNotifier so that it will
  // be cleaned up when the App is destroyed.
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
  FIREBASE_ASSERT(notifier);
  notifier->RegisterObject(rc, [](void* object) {
    RemoteConfig* remote_config = reinterpret_cast<RemoteConfig*>(object);
    LogWarning(
        "RemoteConfig object %p should be deleted before the App %p it "
        "depends upon.",
        remote_config, remote_config->app());
    remote_config->Cleanup();
  });

  GetRcInstances()[app] = rc;
  return rc;
}

RemoteConfig* RemoteConfig::FindRemoteConfig(App* app) {
  MutexLock lock(g_rc_lock);
  if (!g_rc_instances) return nullptr;
  auto it = g_rc_instances->find(app);
  return it != g_rc_instances->end() ? it->second : nullptr;
}

void RemoteConfig::Cleanup() {
  MutexLock lock(g_rc_lock);
  if (!internal_) return;

  if (app_) {
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app_);
    if (notifier) {
      notifier->UnregisterObject(this);
    }
    if (g_rc_instances) {
      g_rc_instances->erase(app_);
      if (g_rc_instances->empty()) {
        delete g_rc_instances;
        g_rc_instances = nullptr;
      }
    }
  }

  internal_->Cleanup();
  delete internal_;
  internal_ = nullptr;
}

RemoteConfig::RemoteConfig(App* app) : app_(app), internal_(nullptr) {}

bool RemoteConfig::InitInternal() {
  MutexLock lock(g_rc_lock);
  if (!app_) return false;
  if (!internal_) {
    internal_ = new internal::RemoteConfigInternal(*app_);
  }
  return internal_->Initialized();
}

RemoteConfig::~RemoteConfig() { Cleanup(); }

Future<ConfigInfo> RemoteConfig::EnsureInitialized() {
  return internal_ ? internal_->EnsureInitialized() : Future<ConfigInfo>();
}

Future<ConfigInfo> RemoteConfig::EnsureInitializedLastResult() const {
  return internal_ ? internal_->EnsureInitializedLastResult()
                   : Future<ConfigInfo>();
}

Future<bool> RemoteConfig::Activate() {
  return internal_ ? internal_->Activate() : Future<bool>();
}

Future<bool> RemoteConfig::ActivateLastResult() const {
  return internal_ ? internal_->ActivateLastResult() : Future<bool>();
}

Future<bool> RemoteConfig::FetchAndActivate() {
  return internal_ ? internal_->FetchAndActivate() : Future<bool>();
}

Future<bool> RemoteConfig::FetchAndActivateLastResult() const {
  return internal_ ? internal_->FetchAndActivateLastResult() : Future<bool>();
}

Future<void> RemoteConfig::Fetch() {
  if (!internal_) return Future<void>();
  return Fetch(GetConfigSettings().minimum_fetch_interval_in_milliseconds /
               1000);
}

Future<void> RemoteConfig::Fetch(uint64_t cache_expiration_in_seconds) {
  return internal_ ? internal_->Fetch(cache_expiration_in_seconds)
                   : Future<void>();
}

Future<void> RemoteConfig::FetchLastResult() const {
  return internal_ ? internal_->FetchLastResult() : Future<void>();
}

#if defined(__ANDROID__)
Future<void> RemoteConfig::SetDefaults(int defaults_resource_id) {
  return internal_ ? internal_->SetDefaults(defaults_resource_id)
                   : Future<void>();
}
#endif  // defined(__ANDROID__)

Future<void> RemoteConfig::SetDefaults(const ConfigKeyValueVariant* defaults,
                                       size_t number_of_defaults) {
  return internal_ ? internal_->SetDefaults(defaults, number_of_defaults)
                   : Future<void>();
}

Future<void> RemoteConfig::SetDefaults(const ConfigKeyValue* defaults,
                                       size_t number_of_defaults) {
  return internal_ ? internal_->SetDefaults(defaults, number_of_defaults)
                   : Future<void>();
}

Future<void> RemoteConfig::SetDefaultsLastResult() const {
  return internal_ ? internal_->SetDefaultsLastResult() : Future<void>();
}

Future<void> RemoteConfig::SetConfigSettings(ConfigSettings settings) {
  return internal_ ? internal_->SetConfigSettings(settings) : Future<void>();
}

ConfigSettings RemoteConfig::GetConfigSettings() const {
  return internal_ ? internal_->GetConfigSettings() : ConfigSettings();
}

Future<void> RemoteConfig::SetConfigSettingsLastResult() const {
  return internal_ ? internal_->SetConfigSettingsLastResult() : Future<void>();
}

bool RemoteConfig::GetBoolean(const char* key) const {
  return GetBoolean(key, static_cast<ValueInfo*>(nullptr));
}

bool RemoteConfig::GetBoolean(const char* key, ValueInfo* info) const {
  return internal_ ? internal_->GetBoolean(key, info) : false;
}

int64_t RemoteConfig::GetLong(const char* key) const {
  return GetLong(key, static_cast<ValueInfo*>(nullptr));
}

int64_t RemoteConfig::GetLong(const char* key, ValueInfo* info) const {
  return internal_ ? internal_->GetLong(key, info) : 0;
}

double RemoteConfig::GetDouble(const char* key) const {
  return GetDouble(key, static_cast<ValueInfo*>(nullptr));
}

double RemoteConfig::GetDouble(const char* key, ValueInfo* info) const {
  return internal_ ? internal_->GetDouble(key, info) : 0.0;
}

std::string RemoteConfig::GetString(const char* key) const {
  return GetString(key, static_cast<ValueInfo*>(nullptr));
}

std::string RemoteConfig::GetString(const char* key, ValueInfo* info) const {
  return internal_ ? internal_->GetString(key, info) : std::string();
}

std::vector<unsigned char> RemoteConfig::GetData(const char* key) const {
  return GetData(key, static_cast<ValueInfo*>(nullptr));
}

std::vector<unsigned char> RemoteConfig::GetData(const char* key,
                                                 ValueInfo* info) const {
  return internal_ ? internal_->GetData(key, info)
                   : std::vector<unsigned char>();
}

std::vector<std::string> RemoteConfig::GetKeysByPrefix(
    const char* prefix) const {
  return internal_ ? internal_->GetKeysByPrefix(prefix)
                   : std::vector<std::string>();
}

std::vector<std::string> RemoteConfig::GetKeys() const {
  return internal_ ? internal_->GetKeys() : std::vector<std::string>();
}

std::map<std::string, Variant> RemoteConfig::GetAll() const {
  return internal_ ? internal_->GetAll() : std::map<std::string, Variant>();
}

ConfigInfo RemoteConfig::GetInfo() const {
  return internal_ ? internal_->GetInfo() : ConfigInfo();
}

Future<void> RemoteConfig::SetCustomSignals(
    const std::map<std::string, Variant>& custom_signals) {
  return internal_ ? internal_->SetCustomSignals(custom_signals)
                   : Future<void>();
}

Future<void> RemoteConfig::SetCustomSignalsLastResult() const {
  return internal_ ? internal_->SetCustomSignalsLastResult() : Future<void>();
}

ConfigUpdateListenerRegistration RemoteConfig::AddOnConfigUpdateListener(
    std::function<void(ConfigUpdate&&, RemoteConfigError)>
        config_update_listener) {
  return internal_ ? internal_->AddOnConfigUpdateListener(
                         std::move(config_update_listener))
                   : ConfigUpdateListenerRegistration();
}

}  // namespace remote_config
}  // namespace firebase
