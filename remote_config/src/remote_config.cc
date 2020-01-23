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

#include "app/src/cleanup_notifier.h"
#include "app/src/mutex.h"
#include "include/firebase/remote_config.h"

// QueryInternal is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "remote_config/src/android/remote_config_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "remote_config/src/ios/remote_config_ios.h"
#else
#include "remote_config/src/desktop/remote_config_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS

namespace firebase {
namespace remote_config {

#ifdef FIREBASE_EARLY_ACCESS_PREVIEW

std::map<App*, RemoteConfig*> g_rcs;
Mutex g_rc_mutex;  // NOLINT

RemoteConfig* RemoteConfig::GetInstance(App* app) {
  MutexLock lock(g_rc_mutex);
  // Return the RemoteConfig if it already exists.
  RemoteConfig* existing_rc = FindRemoteConfig(app);
  if (existing_rc) {
    return existing_rc;
  }

  // Create a new RemoteConfig and initialize.
  RemoteConfig* rc = new RemoteConfig(app);
  LogDebug("Creating RemoteConfig %p for App %s", rc, app->name());

  if (rc->InitInternal()) {
    // Cleanup this object if app is destroyed.
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
    assert(notifier);
    notifier->RegisterObject(rc, [](void* object) {
      RemoteConfig* rc = reinterpret_cast<RemoteConfig*>(object);
      LogWarning(
          "Remote Config object 0x%08x should be deleted before the App 0x%08x "
          "it depends upon.",
          static_cast<int>(reinterpret_cast<intptr_t>(rc)),
          static_cast<int>(reinterpret_cast<intptr_t>(rc->app())));
      rc->DeleteInternal();
    });

    // Stick it in the global map so we remember it, and can delete it on
    // shutdown.
    g_rcs[app] = rc;
    return rc;
  }

  return nullptr;
}

RemoteConfig* RemoteConfig::FindRemoteConfig(App* app) {
  MutexLock lock(g_rc_mutex);
  // Return the RemoteConfig if it already exists.
  std::map<App*, RemoteConfig*>::iterator it = g_rcs.find(app);
  if (it != g_rcs.end()) {
    return it->second;
  }
  return nullptr;
}

void RemoteConfig::DeleteInternal() {
  MutexLock lock(g_rc_mutex);
  if (!internal_) return;

  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app());
  assert(notifier);
  notifier->UnregisterObject(this);

  internal_->Cleanup();
  delete internal_;
  internal_ = nullptr;

  // Remove rc from the g_rcs map.
  g_rcs.erase(app());
}

RemoteConfig::RemoteConfig(App* app) {
  FIREBASE_ASSERT(app != nullptr);
  app_ = app;
  internal_ = new internal::RemoteConfigInternal(*app);
}

RemoteConfig::~RemoteConfig() { DeleteInternal(); }

bool RemoteConfig::InitInternal() { return internal_->Initialized(); }

Future<ConfigInfo> RemoteConfig::EnsureInitialized() {
  return internal_->EnsureInitialized();
}

Future<ConfigInfo> RemoteConfig::EnsureInitializedLastResult() {
  return internal_->EnsureInitializedLastResult();
}

Future<bool> RemoteConfig::Activate() { return internal_->Activate(); }

Future<bool> RemoteConfig::ActivateLastResult() {
  return internal_->ActivateLastResult();
}

Future<bool> RemoteConfig::FetchAndActivate() {
  return internal_->FetchAndActivate();
}

Future<bool> RemoteConfig::FetchAndActivateLastResult() {
  return internal_->FetchAndActivateLastResult();
}

Future<void> RemoteConfig::Fetch() { return Fetch(kDefaultCacheExpiration); }

Future<void> RemoteConfig::Fetch(uint64_t cache_expiration_in_seconds) {
  return internal_->Fetch(cache_expiration_in_seconds);
}

Future<void> RemoteConfig::FetchLastResult() {
  return internal_->FetchLastResult();
}

#if defined(__ANDROID__)
Future<void> RemoteConfig::SetDefaults(int defaults_resource_id) {
  return internal_->SetDefaults(defaults_resource_id);
}
#endif  // __ANDROID__

Future<void> RemoteConfig::SetDefaults(const ConfigKeyValueVariant* defaults,
                                       size_t number_of_defaults) {
  return internal_->SetDefaults(defaults, number_of_defaults);
}

Future<void> RemoteConfig::SetDefaults(const ConfigKeyValue* defaults,
                                       size_t number_of_defaults) {
  return internal_->SetDefaults(defaults, number_of_defaults);
}

Future<void> RemoteConfig::SetDefaultsLastResult() {
  return internal_->SetDefaultsLastResult();
}

Future<void> RemoteConfig::SetConfigSettings(ConfigSettings settings) {
  return internal_->SetConfigSettings(settings);
}

Future<void> RemoteConfig::SetConfigSettingsLastResult() {
  return internal_->SetConfigSettingsLastResult();
}

bool RemoteConfig::GetBoolean(const char* key) {
  return GetBoolean(key, static_cast<ValueInfo*>(nullptr));
}

bool RemoteConfig::GetBoolean(const char* key, ValueInfo* info) {
  return internal_->GetBoolean(key, info);
}

int64_t RemoteConfig::GetLong(const char* key) {
  return GetLong(key, static_cast<ValueInfo*>(nullptr));
}

int64_t RemoteConfig::GetLong(const char* key, ValueInfo* info) {
  return internal_->GetLong(key, info);
}

double RemoteConfig::GetDouble(const char* key) {
  return GetDouble(key, static_cast<ValueInfo*>(nullptr));
}

double RemoteConfig::GetDouble(const char* key, ValueInfo* info) {
  return internal_->GetDouble(key, info);
}

std::string RemoteConfig::GetString(const char* key) {
  return GetString(key, static_cast<ValueInfo*>(nullptr));
}

std::string RemoteConfig::GetString(const char* key, ValueInfo* info) {
  return internal_->GetString(key, info);
}

std::vector<unsigned char> RemoteConfig::GetData(const char* key) {
  return GetData(key, static_cast<ValueInfo*>(nullptr));
}

std::vector<unsigned char> RemoteConfig::GetData(const char* key,
                                                 ValueInfo* info) {
  return internal_->GetData(key, info);
}

std::vector<std::string> RemoteConfig::GetKeysByPrefix(const char* prefix) {
  return internal_->GetKeysByPrefix(prefix);
}

std::vector<std::string> RemoteConfig::GetKeys() {
  return internal_->GetKeys();
}

std::map<std::string, Variant> RemoteConfig::GetAll() {
  return internal_->GetAll();
}

// TODO(b/147143718): Change to a more descriptive name.
const ConfigInfo RemoteConfig::GetInfo() { return internal_->GetInfo(); }
#endif  // FIREBASE_EARLY_ACCESS_PREVIEW

}  // namespace remote_config
}  // namespace firebase
