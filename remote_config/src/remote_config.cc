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

#include "app/src/cleanup_notifier.h"
#include "app/src/mutex.h"
#include "app/src/semaphore.h"
#include "remote_config/src/common.h"
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

std::map<App*, RemoteConfig*> g_rcs;
Mutex g_rc_mutex;  // NOLINT

//----------------- APIs to deprecate --------------------------
static RemoteConfig* g_remote_config_instance = nullptr;

// The semaphore to block the thread and wait for.
static Semaphore* g_future_sem_ = nullptr;

namespace internal {

bool IsInitialized() { return g_remote_config_instance != nullptr; }

}  // namespace internal

InitResult Initialize(const App& app) {
  if (internal::IsInitialized()) return kInitResultSuccess;
  if (g_remote_config_instance == nullptr) {
    g_remote_config_instance = RemoteConfig::GetInstance(
        const_cast<App*>(&app));  // temporary cast to adapting deprecated api.
  }
  g_future_sem_ = new Semaphore(0);
  return kInitResultSuccess;
}

void Terminate() {
  if (!internal::IsInitialized()) return;
  delete g_remote_config_instance;
  g_remote_config_instance = nullptr;
  delete g_future_sem_;
  g_future_sem_ = nullptr;
}

#ifndef SWIG
void SetDefaults(const ConfigKeyValueVariant* defaults,
                 size_t number_of_defaults) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  internal::WaitForFuture<void>(
      g_remote_config_instance->SetDefaults(defaults, number_of_defaults),
      g_future_sem_, "SetDefaults");
}
#endif  // SWIG

#if defined(__ANDROID__)
void SetDefaults(int defaults_resource_id) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  internal::WaitForFuture<void>(
      g_remote_config_instance->SetDefaults(defaults_resource_id),
      g_future_sem_, "SetDefaults");
}
#endif  // defined(__ANDROID__)

void SetDefaults(const ConfigKeyValue* defaults, size_t number_of_defaults) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  g_remote_config_instance->SetDefaults(defaults, number_of_defaults);
}

std::string GetConfigSetting(ConfigSetting setting) {
  FIREBASE_ASSERT_RETURN(std::string(), internal::IsInitialized());
  // Do nothing, function deprecated
  return "";
}

void SetConfigSetting(ConfigSetting setting, const char* value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  // Do nothing, function deprecated
}

bool GetBoolean(const char* key) {
  return GetBoolean(key, static_cast<ValueInfo*>(nullptr));
}

bool GetBoolean(const char* key, ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  return g_remote_config_instance->GetBoolean(key, info);
}

int64_t GetLong(const char* key) {
  return GetLong(key, static_cast<ValueInfo*>(nullptr));
}

int64_t GetLong(const char* key, ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(0, internal::IsInitialized());
  return g_remote_config_instance->GetLong(key, info);
}

double GetDouble(const char* key) {
  return GetDouble(key, static_cast<ValueInfo*>(nullptr));
}

double GetDouble(const char* key, ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(0.0, internal::IsInitialized());
  return g_remote_config_instance->GetDouble(key, info);
}

std::string GetString(const char* key) {
  return GetString(key, static_cast<ValueInfo*>(nullptr));
}

std::string GetString(const char* key, ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(std::string(), internal::IsInitialized());
  return g_remote_config_instance->GetString(key, info);
}

std::vector<unsigned char> GetData(const char* key) {
  return GetData(key, static_cast<ValueInfo*>(nullptr));
}

std::vector<unsigned char> GetData(const char* key, ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(std::vector<unsigned char>(),
                         internal::IsInitialized());
  return g_remote_config_instance->GetData(key, info);
}

std::vector<std::string> GetKeysByPrefix(const char* prefix) {
  FIREBASE_ASSERT_RETURN(std::vector<std::string>(), internal::IsInitialized());
  return g_remote_config_instance->GetKeysByPrefix(prefix);
}

std::vector<std::string> GetKeys() {
  FIREBASE_ASSERT_RETURN(std::vector<std::string>(), internal::IsInitialized());
  return g_remote_config_instance->GetKeys();
}

bool ActivateFetched() {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  Future<bool> activate_future = g_remote_config_instance->Activate();
  internal::WaitForFuture<bool>(activate_future, g_future_sem_, "Activate");
  return *activate_future.result();
}

const ConfigInfo& GetInfo() {
  static ConfigInfo config_info;
  FIREBASE_ASSERT_RETURN(config_info, internal::IsInitialized());
  config_info = g_remote_config_instance->GetInfo();
  return config_info;
}

Future<void> Fetch() { return Fetch(kDefaultCacheExpiration); }

Future<void> Fetch(uint64_t cache_expiration_in_seconds) {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  return g_remote_config_instance->Fetch(cache_expiration_in_seconds);
}

Future<void> FetchLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  return g_remote_config_instance->FetchLastResult();
}

//----------------- APIs to deprecate END --------------------------

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

Future<void> RemoteConfig::Fetch() { return Fetch(GetConfigFetchInterval()); }

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

ConfigSettings RemoteConfig::GetConfigSettings() {
  return internal_->GetConfigSettings();
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

uint64_t RemoteConfig::GetConfigFetchInterval() {
  uint64_t cache_time =
      GetConfigSettings().minimum_fetch_interval_in_milliseconds;
  if (cache_time == 0) {
    cache_time = kDefaultCacheExpiration;
  }
  return cache_time;
}

}  // namespace remote_config
}  // namespace firebase
